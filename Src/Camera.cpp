#include "Camera.h"
#include "Utilities.h"


namespace Niagara
{
	// Globals
	
	// FIXME:
	CameraManipulator::Inputs g_Inputs{};


	CameraManipulator::CameraManipulator() 
	{
		Update();
	}

	// Set the new camera as a target
	void CameraManipulator::SetCamera(const Camera& camera, bool bInstantSet)
	{
		m_DoingAnim = false;

		if (bInstantSet)
		{
			m_Current = camera;
			Update();
		}
		else if (m_Current != camera)
		{
			m_Target = camera;
			m_Snapshot = m_Current;
			m_DoingAnim = true;
			m_StartTime = static_cast<float>(GetSystemTime());
			FindBezierPoints();
		}
	}

	// To call when the mouse is moving
	// It find the appropriate camera operator, based on the mouse button pressed and the keyboard modifiers (shift, ctrl, alt)
	// Returns the action that was activated
	CameraManipulator::Actions CameraManipulator::MouseMove(int x, int y, const Inputs& inputs)
	{
		if (!inputs.lmb && !inputs.rmb && !inputs.mmb)
		{
			SetMousePosition(x, y);
			return Actions::NoAction; // no mouse button pressed
		}

		Actions curAction = Actions::NoAction;
		if (inputs.lmb)
		{
			if (((inputs.ctrl) && (inputs.shift)) || inputs.alt)
				curAction = m_Mode == Modes::Examine ? Actions::LookAround : Actions::Orbit;
			else if (inputs.shift)
				curAction = Actions::Dolly;
			else if (inputs.ctrl)
				curAction = Actions::Pan;
			else
				curAction = m_Mode == Modes::Examine ? Actions::Orbit : Actions::LookAround;
		}
		else if (inputs.mmb)
			curAction = Actions::Pan;
		else if (inputs.rmb)
			curAction = Actions::Dolly;

		if (curAction != Actions::NoAction)
			Motion(x, y, curAction);

		return curAction;
	}

	// Creats a viewing matrix derived from an eye point, a reference point indicating the center of the scene and an up vector
	void CameraManipulator::SetLookat(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up, bool bInstantSet)
	{
		Camera camera{ eye, center, up, m_Current.fov };
		SetCamera(camera, bInstantSet);
	}

	// Modify the position of the camera over time
	// - The camera can be updated through keys. A key set a direction which is added to both eye and center, until the key is released
	// - A new position of the camera defined and the camera will reach that position over time.
	void CameraManipulator::UpdateAnim()
	{
		auto currentTime = static_cast<float>(GetSystemTime());
		auto elapse = (currentTime - m_StartTime) / 1000.0f;

#if 0
		// Key animation
		if (m_KeyVec != glm::vec3(0))
		{
			auto delta = m_KeyVec * elapse;
			m_Current.eye += m_KeyVec * elapse;
			m_Current.center += m_KeyVec * elapse;
			Update();
			m_StartTime = currentTime;
			return;
		}
#endif

		// Camera moving to new position
		if (!m_DoingAnim)
			return;

		float t = std::min(elapse / float(m_Duration), 1.0f);
		// Evalutate polynomial (smoother step from Perline)
		t = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		if (t >= 1.0f)
		{
			m_Current = m_Target;
			m_DoingAnim = false;
			return;
		}

		// Interpolate camera position and interest 
		// The distance of the camera between the interest is preserved to create a nicer interpolation
		// glm::vec3 vpos, vint, vup;
		m_Current.center = glm::mix(m_Snapshot.center, m_Target.center, t);
		m_Current.up = glm::mix(m_Snapshot.up, m_Target.up, t);
		m_Current.eye = ComputeBezier(t, m_Bezier[0], m_Bezier[1], m_Bezier[2]);
		m_Current.fov = glm::mix(m_Snapshot.fov, m_Target.fov, t);

		Update();
	}

	// Low level function for when the camera move
	void CameraManipulator::Motion(int x, int y, Actions action)
	{
		float dx = float(x - m_Mouse[0]) / float(m_Width);
		float dy = float(y - m_Mouse[1]) / float(m_Height);

		switch (action)
		{
		case Actions::Orbit:
			Orbit(dx, dy, false);
			break;
		case Actions::Dolly:
			Dolly(dx, dy);
			break;
		case Actions::Pan:
			Pan(dx, dy);
			break;
		case Actions::LookAround:
			Orbit(dx, -dy, true);
			break;
		}

		// Resetting animation
		m_DoingAnim = false;

		Update();

		m_Mouse[0] = static_cast<float>(x);
		m_Mouse[1] = static_cast<float>(y);
	}

	// Function for when the camera move with keys
	void CameraManipulator::KeyMotion(float dx, float dy, Actions action)
	{
		if (action == Actions::NoAction)
		{
			m_KeyVec = glm::vec3(0);
			return;
		}

		auto d = glm::normalize(m_Current.center - m_Current.eye);
		dx *= m_Speed * 2.0f;
		dy *= m_Speed * 2.0f;

		glm::vec3 keyVec{};
		if (action == Actions::Dolly)
		{
			keyVec = d * dx;
			if (m_Mode == Modes::Walk)
			{
				if (m_Current.up.y > m_Current.up.z)
					keyVec.y = 0;
				else
					keyVec.z = 0;
			}
		}
		else if (action == Actions::Pan)
		{
			auto r = glm::cross(d, m_Current.up);
			keyVec = r * dx + m_Current.up * dy;
		}

		m_KeyVec += keyVec;

		// Key animation
		{
			m_Current.eye += m_KeyVec;
			m_Current.center += m_KeyVec;
			Update();
		}

		// Resetting animation
		m_StartTime = static_cast<float>(GetSystemTime());
	}

	// Trigger a dolly when the wheel change
	void CameraManipulator::Wheel(int value, const Inputs& inputs)
	{
		float fval = static_cast<float>(value);
		float dx = (fval * fabsf(fval)) / static_cast<float>(m_Width);

		if (inputs.shift)
		{
			SetFov(m_Current.fov + fval);
		}
		else 
		{
			Dolly(dx * m_Speed, dx * m_Speed);
			Update();
		}
	}

	// Set and clamp FOV between 0.01 and 179 degrees
	void CameraManipulator::SetFov(float fov)
	{
		m_Current.fov = std::min(std::max(fov, 0.01f), 179.0f);
	}

	void CameraManipulator::Update()
	{
		m_ViewMatrix = glm::lookAt(m_Current.eye, m_Current.center, m_Current.up);
		float aspect = (float)m_Width / (float)m_Height;
#if 0
		m_ProjMatrix = glm::perspective(m_Current.fov, aspect, m_ClipPlanes.x, m_ClipPlanes.y);
#else
		m_ProjMatrix = Niagara::MakeInfReversedZProjRH(m_Current.fov, aspect, m_ClipPlanes.x);
#endif
		m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
	}

	// Pan the camera perpendicularly to the light of sight
	void CameraManipulator::Pan(float dx, float dy)
	{
		if (m_Mode == Modes::Fly)
		{
			dx *= -1;
			dy *= -1;
		}

		glm::vec3 z(m_Current.eye - m_Current.center);
		float length = glm::length(z) / 0.785f; // PI/4 ???
		z = glm::normalize(z);
		glm::vec3 x = glm::normalize(glm::cross(m_Current.up, z));
		glm::vec3 y = glm::normalize(glm::cross(z, x));
		x *= -dx * length;
		y *=  dy * length;

		m_Current.eye += x + y;
		m_Current.center += x + y;
	}

	// Orbit the camera around the center of interest. If `invert` is true,,
	// then the camera stays in place and the interest orbit around the camera
	void CameraManipulator::Orbit(float dx, float dy, bool bInverse)
	{
		if (dx == 0 && dy == 0)
			return;

		// Full width will do a full turn
		dx *= glm::two_pi<float>();
		dy *= glm::two_pi<float>();

		// Get the camera
		glm::vec3 origin{ bInverse ? m_Current.eye : m_Current.center };
		glm::vec3 position{ bInverse ? m_Current.center : m_Current.eye };

		// Get the length of sight
		glm::vec3 centerToEye{ position - origin };
		float radius = glm::length(centerToEye);
		centerToEye = glm::normalize(centerToEye);

		glm::mat4 rotX, rotY;

		// Find the rotation around the Up axis (Y)
		glm::vec3 axeZ = centerToEye;
		rotY = glm::rotate(glm::mat4(1.0f), -dx, m_Current.up);

		// Apply the Y rotation to the eye-center vector
		centerToEye = rotY * glm::vec4(centerToEye, 0);
		
		// Find the rotation around the X vector: cross between eye-center and up (X)
		glm::vec3 axeX = glm::normalize(glm::cross(m_Current.up, axeZ));
		rotX = glm::rotate(glm::mat4(1.0f), -dy, axeX);

		// Apply the (X) rotation to the eye-center vector
		glm::vec3 vecRot = rotX * glm::vec4(centerToEye, 0);
		if (glm::sign(vecRot.x) == glm::sign(centerToEye.x))
			centerToEye = vecRot;

		// Make the vector as long as it was originally
		centerToEye *= radius;

		// Finding the new position
		glm::vec3 newPosition = centerToEye + origin;

		if (!bInverse)
			m_Current.eye = newPosition; // normal: change the position of the camera
		else
			m_Current.center = newPosition; // inverted: change the interest point
	}

	// Make the camera toward the interest point, but don't cross it
	void CameraManipulator::Dolly(float dx, float dy)
	{
		glm::vec3 z = m_Current.center - m_Current.eye;
		float length = glm::length(z);

		// We are at the point of interest, and don't know any direction, so do nothing!
		if (length < EPS)
			return;

		// Use the larger movement
		float dd;
		if (m_Mode == Modes::Examine)
			dd = -dy;
		else
			dd = fabs(dx) > fabs(dy) ? dx : -dy;

		float factor = m_Speed * dd;

		// Adjust speed based on distance
		if (m_Mode == Modes::Examine)
		{
			// Don't move over the point of interest
			if (factor >= 1.0f)
				return;

			z *= factor;
		}
		else
		{
			// Normalize the Z vector and make it faster
			z *= factor / length * 10.0f;
		}

		// Not going up
		if (m_Mode == Modes::Walk)
			if (m_Current.up.y > m_Current.up.z)
				z.y = 0;
			else
				z.z = 0;

		m_Current.eye += z;

		// In fly mode, the interest moves with us
		if (m_Mode == Modes::Examine)
			m_Current.center += z;
	}

	glm::vec3 CameraManipulator::ComputeBezier(float t, glm::vec3& p0, glm::vec3& p1, glm::vec3& p2)
	{
		float u = 1.0f - t;
		float t2 = t * t;
		float u2 = u * t;

		glm::vec3 p = u2 * p0;	// first term
		p += 2 * u * t * p1;	// second term
		p += t2 * p2;			// third term

		return p;
	}

	void CameraManipulator::FindBezierPoints()
	{
		const glm::vec3& p0 = m_Current.eye;
		const glm::vec3& p2 = m_Target.eye;
		glm::vec3 p1, pc;

		// Point of interest
		glm::vec3 pi = (m_Target.center + m_Current.center) * 0.5f;

		glm::vec3 p02 = (p0 + p2) * 0.5f; // mid p0-p2
		float radius = (glm::length(p0 - pi) + glm::length(p2 - pi)) * 0.5f; // radius for p1
		glm::vec3 p02pi(glm::normalize(p02 - pi));
		p02pi *= radius;
		pc = pi + p02pi; // calculated point to go through
		p1 = 2.f * pc - p0 * 0.5f - p2 * 0.5f; // computing t1 for t=0.5
		p1.y = p02.y; // clamping the p1 to be in the same height as p0-p2

		m_Bezier[0] = p0;
		m_Bezier[1] = p1;
		m_Bezier[2] = p2;		
	}

}
