#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
/**
 * Basic camera class.
 * Requires GLM (or refactor with your own math library)
 */
class Camera
{
public:
    
    Camera();
    Camera( int screenWidth, int screenHeight );
    glm::vec4 GetViewport() const;
	//creates and sets a projection matrix
    void SetProjectionRH( float fov, float aspectRatio, float zNear, float zFar );
	//creates an orthographic projecction matrix
	glm::mat4 CreateOrthrographicProjectionMatrix(float regionSize)
	{
		glm::mat4 ProjectionMatrix = glm::ortho(-regionSize / 5.0f, regionSize / 5.0f, -regionSize / 5.0f, regionSize / 5.0f, 0.0f, regionSize*10.0f);
		return ProjectionMatrix;
	}
    void ApplyViewMatrix();
    void SetPosition( const glm::vec3& pos );
    glm::vec3 GetPosition() const;
    // Translate the camera by some amount. If local is TRUE (default) then the translation should
    // be applied in the local-space of the camera. If local is FALSE, then the translation is 
    // applied in world-space.
    void Translate( const glm::vec3& delta, bool local = true );
	//set rotation in quaternion
    void SetRotation( const glm::quat& rot );
	//get rotation in quaternion
    glm::quat GetRotation() const;
	//set euler angles
    void SetEulerAngles( const glm::vec3& eulerAngles );
	//get rotatio in euler angles
    glm::vec3 GetEulerAngles() const;
    // Rotate the camera by some amount.
    void Rotate( const glm::quat& rot );
	//extract forward vector from view matrix
	glm::vec3 GetForwardVector();
	//extract up vector from view matrix
	glm::vec3 GetUpVector();
	//get the projection matrix
    glm::mat4 GetProjectionMatrix();
	//get the view matrix
    glm::mat4 GetViewMatrix();
	//Get field of view(fov)
	float GetFOV(){
		return m_fov;
	}
	glm::mat4 m_IViewMatrix;
	glm::mat4 m_PrevIViewMatrix;
protected:
	//update the view matrix
	void UpdateViewMatrix();
    glm::vec4 m_Viewport;
    glm::quat m_Rotation;
    glm::mat4 m_ViewMatrix;
	
    glm::mat4 m_ProjectionMatrix;
	glm::vec3 m_Position;
	//glm::lookAt
	float m_fov;

private:
    bool m_ViewDirty;
};

class LightCamera
{
public:


};

#endif//CAMERA_H