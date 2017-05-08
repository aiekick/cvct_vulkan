#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define OUTPUTVERTICES 3
#define EPS 0.0000001

//properties
layout(triangles) in;                                                                  
layout(triangle_strip,max_vertices = OUTPUTVERTICES) out;          

//input
layout(location = 0) in vec2 inTex[OUTPUTVERTICES];
layout(location = 1) in vec3 inWPos[OUTPUTVERTICES];
layout(location = 2) in vec3 inWNormal[OUTPUTVERTICES];
layout(location = 3) in vec3 inWTan[OUTPUTVERTICES];
layout(location = 4) in vec3 inWBitan[OUTPUTVERTICES];

//output locations
layout(location = 0) out vec2 outTex;
layout(location = 1) out vec3 outWPos;
layout(location = 2) out vec3 outWNormal;
layout(location = 3) out vec3 outWTan;
layout(location = 4) out vec3 outWBitan;

//uniform buffers
layout (set = 2, binding = 1) uniform UBO 
{
	mat4 ViewProjectionXY;	//X axis
	mat4 ViewProjectionXZ;	//y axis	
	mat4 ViewProjectionYZ;	//Z axis
} ubo;

void main()
{
	//get the triangle verts in world space
	vec4 pos0 = gl_in[0].gl_Position;
	vec4 pos1 = gl_in[1].gl_Position;
	vec4 pos2 = gl_in[2].gl_Position;

	//get the triangle normals
	vec3 norm0 = inWNormal[0];
	vec3 norm1 = inWNormal[1];
	vec3 norm2 = inWNormal[2];

	//calculate triangle normal
	vec3 p0 = vec3(pos1-pos0);
	vec3 p1 = vec3(pos2-pos0);
	vec3 faceNormal = normalize(cross(p0,p1));
	vec3 vertexNormal = normalize(norm0+norm1+norm2);
	float dotp = dot(faceNormal, vertexNormal);
	faceNormal =  (dotp < 0.0) ? -faceNormal : faceNormal;

	//calculate the dominant axis
	mat4 uViewProjection;
	float axisX = abs(dot(vec3(1,0,0),faceNormal));
	float axisY = abs(dot(vec3(0,1,0),faceNormal));
	float axisZ = abs(dot(vec3(0,0,1),faceNormal));
	//X axis dominant
	if(axisX > max(axisY,axisZ))
	{
		uViewProjection = ubo.ViewProjectionYZ;
	}
	//Y axis dominant
	else if(axisY > max(axisX,axisZ))
	{
		uViewProjection = ubo.ViewProjectionXZ;
	}
	//Z axis dominant
	else if(axisZ > max(axisX,axisY))
	{
		uViewProjection = ubo.ViewProjectionXY;
	}
	
	//transform the verts with their dominant axis view projection matrix
	pos0 = uViewProjection * pos0;
	pos1 = uViewProjection * pos1;
	pos2 = uViewProjection * pos2;

	//bloat the triangles TODO:
	vec4 dir0 = pos0-pos1;
	vec4 dir1 = pos1-pos0;
	vec4 dir2 = pos2-pos0;
	pos0+= dir0*EPS;
	pos1+= dir1*EPS;
	pos2+= dir2*EPS;
	
//create the triangle
	gl_Position = pos0;
		outTex = inTex[0];
		outWPos = inWPos[0];
		outWNormal = inWNormal[0];
		outWTan = inWTan[0];
		outWBitan = inWBitan[0];
EmitVertex();
	gl_Position = pos1;
		outTex = inTex[1];
		outWPos = inWPos[1];
		outWNormal = inWNormal[1];
		outWTan = inWTan[1];
		outWBitan = inWBitan[1];
EmitVertex();
	gl_Position = pos2;
		outTex = inTex[2];
		outWPos = inWPos[2];
		outWNormal = inWNormal[2];
		outWTan = inWTan[2];
		outWBitan = inWBitan[2];
EmitVertex();
}