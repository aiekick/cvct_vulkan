#include "Camera.h"

Camera::Camera()
    : m_Viewport(0)
    , m_Position(0)
    , m_Rotation()
    , m_ProjectionMatrix(1)
    , m_ViewMatrix(1)
	, m_IViewMatrix(glm::inverse(m_ViewMatrix))
	, m_PrevIViewMatrix(m_IViewMatrix)
    , m_ViewDirty(false)
{}

Camera::Camera(int screenWidth, int screenHeight)
	: m_Viewport(0, 0, screenWidth, screenHeight)
	, m_Position(0)
	, m_Rotation()
	, m_ProjectionMatrix(1)
	, m_ViewMatrix(1)
	, m_IViewMatrix(glm::inverse(m_ViewMatrix))
	, m_PrevIViewMatrix(m_IViewMatrix)
    , m_ViewDirty( false )
{
}

glm::vec4 Camera::GetViewport() const
{
    return m_Viewport;
}

void Camera::SetProjectionRH( float fov, float aspectRatio, float zNear, float zFar )
{
	m_fov = fov;
    m_ProjectionMatrix = glm::perspective( fov, aspectRatio, zNear,zFar );
}

void Camera::ApplyViewMatrix()
{
    UpdateViewMatrix();
}

void Camera::SetPosition( const glm::vec3& pos )
{
    m_Position = pos;
    m_ViewDirty = true;
}

glm::vec3 Camera::GetPosition() const
{
    return m_Position;
}

void Camera::Translate( const glm::vec3& delta, bool local /* = true */ )
{
    if ( local )
    {
        m_Position += m_Rotation * delta;
    }
    else
    {
        m_Position += delta;
    }
    m_ViewDirty = true;
}

void Camera::SetRotation( const glm::quat& rot )
{
    m_Rotation = rot;
    m_ViewDirty = true;
}

glm::quat Camera::GetRotation() const
{
    return m_Rotation;
}

void Camera::SetEulerAngles( const glm::vec3& eulerAngles )
{
    m_Rotation = glm::quat(glm::radians(eulerAngles));
}

glm::vec3 Camera::GetEulerAngles() const
{
    return glm::degrees(glm::eulerAngles( m_Rotation ));
}

void Camera::Rotate( const glm::quat& rot )
{
    m_Rotation = m_Rotation * rot;
    m_ViewDirty = true;
}

glm::mat4 Camera::GetProjectionMatrix()
{
    return m_ProjectionMatrix;
}

glm::mat4 Camera::GetViewMatrix()
{
    UpdateViewMatrix();
    return m_ViewMatrix;
}

glm::vec3 Camera::GetForwardVector()
{
	UpdateViewMatrix();
	glm::vec3 ret(m_ViewMatrix[0][2], m_ViewMatrix[1][2], m_ViewMatrix[2][2]);
	return glm::normalize(-ret);
}

glm::vec3 Camera::GetUpVector()
{
	UpdateViewMatrix();
	glm::vec3 ret(m_ViewMatrix[0][1], m_ViewMatrix[1][1], m_ViewMatrix[2][1]);
	return glm::normalize(ret);
}

void Camera::UpdateViewMatrix()
{
    if ( m_ViewDirty )
    {
        glm::mat4 translate = glm::translate(-m_Position);
        // Since we know the rotation matrix is orthonormalized, we can simply 
        // transpose the rotation matrix instead of inversing.
        glm::mat4 rotate = glm::transpose(glm::toMat4(m_Rotation));

		//store old values first
		m_PrevIViewMatrix = m_IViewMatrix;

		//set the view matrix
        m_ViewMatrix = rotate * translate;
		m_IViewMatrix = glm::inverse(m_IViewMatrix);

		//set viewdirty to false
        m_ViewDirty = false;
    }
}
