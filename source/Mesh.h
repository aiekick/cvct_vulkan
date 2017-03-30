#ifndef MESH_H
#define MESH_H

#include "glm\glm.hpp"
namespace VKTools
{
	extern Mesh* LoadMesh(const char* filePath, const char* fileFolder);

};

class Mesh
{
public:
	Mesh();
	~Mesh();

	void Transform(glm::vec3 translation, glm::vec3 rotation, float angle, glm::vec3 scale);

private:

};

#endif	//MESH_H