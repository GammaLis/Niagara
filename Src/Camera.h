// Ref: nvpro_core - cameramanipulater

#pragma once

#include "pch.h"
#include "Utilities.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
* CameraManipulator is a camera manipulator help class,
* It allows to do
* - Orbit		(LMB)
* - Pan			(LMB + CTRL		| MMB)
* - Dolly		(LMB + SHIFT	| RMB)
* - Look around (LMB + ALT		| LMB + CTRL + SHIFT)
*/

namespace Niagara
{
	class CameraManipulator
	{
	public:
		enum class Modes { Examine, Fly, Walk };
		enum class Actions { NoAction, Orbit, Dolly, Pan, LookAround };
		struct Inputs
		{
			bool lmb = false; bool mmb = false; bool rmb = false;
			bool shift = false; bool ctrl = false; bool alt = false;
		};

		struct Camera 
		{
			glm::vec3 eye{ 10, 10, 10 };
			glm::vec3 center{};
			glm::vec3 up{ 0, 1, 0 };
			float fov = 60.0f;

			bool operator != (const Camera& other) const 
			{
				return (eye != other.eye) || (center != other.center) || (up != other.up) || (fov != other.fov);
			}
			bool operator == (const Camera& other) const
			{
				return (eye == other.eye) && (center == other.center) && (up == other.up) && (fov == other.fov);
			}
		};

		static CameraManipulator& Singleton()
		{
			static CameraManipulator cameraManipulator;
			return cameraManipulator;
		}

	protected:
		CameraManipulator();

	public:
		// Main function to call from this application
		// On application mouse move, call this function with the current mouse position, mouse button presses and 
		// keyboard modifier. The camera matrix will be updated.
		Actions MouseMove(int x, int y, const Inputs& inputs);

		// Set the camera to look at the interest point
		// bInstantSet = true will not interpolate to the new position
		void SetLookat(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up, bool bInstantSet = true);

		// This should be called in an application loop to update the camera matrix if this one is animated: new position, key movement
		void UpdateAnim();

		const Camera& GetCamera() const { return m_Current; }
		void SetCamera(const Camera& camera, bool bInstantSet = true);

		Modes GetMode() const { return m_Mode; }
		// Set the manipulator mode, from Examiner, to walk, to fly, ...
		void SetMode(Modes mode) { m_Mode = mode; }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetProjMatrix() const { return m_ProjMatrix; }
		const glm::mat4& GetViewProjMatrix() const { return m_ViewProjMatrix; }

		// To call when the size of the window change. This allows to do nicer movement according to the window size.
		void SetWindowSize(int w, int h)
		{
			m_Width = w; 
			m_Height = h;
		}

		void SetSpeed(float speed) { m_Speed = speed; }

		void GetMousePosition(int& x, int& y)
		{
			x = static_cast<int>(m_Mouse[0]);
			y = static_cast<int>(m_Mouse[1]);
		}
		// Setting the current mouse position, to call on mouse button down. Allow to compute properly the deltas.
		void SetMousePosition(int x, int y)
		{
			m_Mouse = { static_cast<float>(x), static_cast<float>(y) };
		}

		// Main function which is called to apply a camera motion
		void Motion(int x, int y, Actions action = Actions::NoAction);

		void KeyMotion(float dx, float dy, Actions action);

		void Wheel(int value, const Inputs& inputs);

		void SetFov(float fov);

		// Animation duration
		void SetAnimationDuration(float duration) { m_Duration = duration; }
		bool IsAnimated() const { return m_DoingAnim == true; }

		// TODO:
		// Fitting the camera position and interest to see the bounding box
		void Fit() {  }

	private:
		// Update the internal matrix
		void Update();

		// Panning: movement parallels to the screen
		void Pan(float dx, float dy);
		// Orbiting: rotation around the center of interest. If invert, the interset orbit around the camera position
		void Orbit(float dx, float dy, bool bInverse = false);
		// Dolly: movement toward the interest
		void Dolly(float dx, float dy);

		glm::vec3 ComputeBezier(float t, glm::vec3& p0, glm::vec3& p1, glm::vec3& p2);
		void FindBezierPoints();

	public:
		// FIXME: These should be private, but now just set them public
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjMatrix;
		glm::mat4 m_ViewProjMatrix;
		// TODO: 
		// m_PrevVPMatrix, m_JitteredProjMatrix;

		Camera m_Current;	// Current camera position
		Camera m_Target;	// Wish camera position
		Camera m_Snapshot;	// Current camera the moment a set look-at is done

		// Animation
		std::array<glm::vec3, 3> m_Bezier;
		float m_StartTime = 0;
		float m_Duration = 0;
		bool m_DoingAnim = false;
		glm::vec3 m_KeyVec{};

		// Screen
		int m_Width = 1;
		int m_Height = 1;

		float m_Speed = 1.0f;
		glm::vec2 m_Mouse{};
		glm::vec2 m_ClipPlanes{ 0.01f, 1000.0f };

		bool m_Button = false;	// button pressed
		bool m_Moving = false;	// mouse is moving
		float m_TbSize = 0.8f;	// trackball size

		Modes m_Mode{ Modes::Examine };
	};

	extern CameraManipulator::Inputs g_Inputs;


	class Camera
	{
	public:
		enum class CameraType
		{
			Lookat,
			FirstPerson
		};

		void SetPerspective(float fov, float aspect, float znear, float zfar)
		{
			this->fov = fov;
			this->znear = znear;
			this->zfar = zfar;
			m_ProjectionMatrix = glm::perspective(glm::radians(fov), aspect, znear, zfar);
			if (flipY)
				m_ProjectionMatrix[1][1] *= -1.0f;
		}

		void Update(float deltaTime)
		{
			updated = false;
			if (cameraType == CameraType::FirstPerson)
			{
				// if ()
			}
		}

		void UpdateViewMatrix()
		{
			glm::mat4 rotMat = glm::mat4(1.0f);
			glm::mat4 transMat{};

			// rotMat = glm::rotate(rotMat, glm::)
		}

		void UpdateAspectRatio(float aspect)
		{
			m_ProjectionMatrix = glm::perspective(glm::radians(fov), aspect, znear, zfar);
			if (flipY)
				m_ProjectionMatrix[1][1] *= -1.0f;
		}

		void UpdatePosition(const glm::vec3& position)
		{
			this->position = position;
			UpdateViewMatrix();
		}

		void UpdateRotation(const glm::vec3& rotation)
		{
			this->rotation = rotation;
			UpdateViewMatrix();
		}

		void Rotate(const glm::vec3& delta)
		{
			this->rotation += delta;
			UpdateViewMatrix();
		}

		void UpdateTranslate(const glm::vec3& translation)
		{
			this->position = translation;
			UpdateViewMatrix();
		}

		void Translate(const glm::vec3& delta)
		{
			this->position += delta;
			UpdateViewMatrix();
		}

		CameraType cameraType = CameraType::Lookat;

		glm::vec3 rotation{};
		glm::vec3 position{};
		glm::vec3 forward{ 0.0f, 0.0f, -1.0f };

		float rotationSpeed = 1.0f;
		float movementSpeed = 1.0f;

		bool updated = false;
		bool flipY = false;

		float fov = 90.0f;
		float znear = 0.1f, zfar = 100.0f;

	private:
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix;
	};
}
