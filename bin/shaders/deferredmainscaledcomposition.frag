#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define COLOR_IMAGE_COUNT 6.0

#define EPS		0.0001
#define PI		3.14159265
#define TWOPI	6.28318530

const int MAX_STEPS = 300;
#define STEPSIZE_WRT_TEXEL 0.3333333333  // Cyril uses 1/3
//#define STEPSIZE_WRT_TEXEL 1.0  // Cyril uses 1/3
#define AO_DIST_K 0.8
#define INDIR_DIST_K 0.01
#define MAXCASCADE 10

layout (location = 0) in vec2 inUV;

layout (input_attachment_index = 1, set = 2, binding = 3) uniform subpassInput samplerposition;
layout (input_attachment_index = 2, set = 2, binding = 4) uniform subpassInput samplerNormal;
layout (input_attachment_index = 3, set = 2, binding = 5) uniform subpassInput samplerAlbedo;
layout (input_attachment_index = 4, set = 2, binding = 6) uniform subpassInput samplerTangent;

//output
layout (location = 0) out vec4 outColor;

//set bindings
layout(set = 2, binding = 1) uniform sampler3D rVoxelColor;					// Read
layout(set = 2, binding = 2) writeonly uniform image2D scaledOutput;		// write

layout (set = 2, binding = 0) uniform UBO 
{
	vec4 voxelRegionWorld[MAXCASCADE];	// list of voxel region worlds
	vec3 cameraPosition;				// Positon of the camera adsfasdfa
	uint voxelGridResolution;			// Resolution of the voxel grid
	uint cascadeCount;					// Number of cascades

	float scaledWidth;					// Scaled width of framebuffer
	float scaledHeight;					// Scaled height of framebuffer
	uint conecount;						// The selected renderer
	uint renderer;				// renderer
	vec3 padding0;
} ubo;

// Global variables
float gvoxelSize = 0;
float gsideoffset = 0.16666666666;	// 1.0/6.0
uint gstartCascade = 0;
float gperCascade = 0;

// rotate vector a given angle(rads) over a given axis
// https://github.com/MoSync/MoSync/blob/master/examples/cpp/Graphun/vec.h
vec3 Rotate(vec3 vector, float angle, vec3 axis)
{
    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0 - c;

    mat3 rot;
    rot[0][0] = c + axis.x*axis.x*t;
    rot[1][1] = c + axis.y*axis.y*t;
    rot[2][2] = c + axis.z*axis.z*t;

    float tmp1 = axis.x*axis.y*t;
    float tmp2 = axis.z*s;
    rot[1][0] = tmp1 + tmp2;
    rot[0][1] = tmp1 - tmp2;
    tmp1 = axis.x*axis.z*t;
    tmp2 = axis.y*s;
    rot[2][0] = tmp1 - tmp2;
    rot[0][2] = tmp1 + tmp2;
    tmp1 = axis.y*axis.z*t;
    tmp2 = axis.x*s;
    rot[2][1] = tmp1 + tmp2;
    rot[1][2] = tmp1 - tmp2;

    return rot*vector;
}

// Anisotropic cube sampler
vec4 SampleAnisotropic(vec3 pos, vec3 dir, uint cascade, float miplevel)
{
	uvec3 isPositive = uvec3(dir.x > 0.0 ? 1:0, dir.y > 0.0 ? 1:0, dir.z > 0.0 ? 1:0);
	vec3 scalar = abs(dir);

	// Calculate the cascade offset
	float maxperCascade = (1.0/float(ubo.cascadeCount));
	float cascadeoffset = maxperCascade * cascade;
	// resize the pos range
	vec3 tpos = vec3(pos.x / COLOR_IMAGE_COUNT, pos.y / ubo.cascadeCount, pos.z);

	miplevel = min(miplevel,2);

	vec4 xtexel = textureLod(rVoxelColor, tpos + vec3(gsideoffset*float(isPositive.x+0),cascadeoffset,0), miplevel);
	vec4 ytexel = textureLod(rVoxelColor, tpos + vec3(gsideoffset*float(isPositive.y+2),cascadeoffset,0), miplevel);
	vec4 ztexel = textureLod(rVoxelColor, tpos + vec3(gsideoffset*float(isPositive.z+4),cascadeoffset,0), miplevel);

    return (scalar.x*xtexel + scalar.y*ytexel + scalar.z*ztexel);
}

//indirect cone tracing
// ray origin, ray direction, cone theta(radians)
vec4 conetraceIndir(vec3 ro, vec3 rd, float theta, float scalar)
{
    vec3 rayOrigin = ro;
    float dist = 0.0;
	
	// Cone aperture at distance 1.0
    float coneTheta = tan(theta);
	// Accumulated color
    vec3 acol = vec3(0.0);   
	// Accumulated transmittance
	float atm = 0.0;	
	float a = 0.0;
	
	// Accumulated stepping size
	vec3 stepped = vec3(0);

	uint currentCascade = gstartCascade;
	// Max positions
	vec3 minpos = ubo.voxelRegionWorld[currentCascade].xyz;
	vec3 maxpos = minpos + ubo.voxelRegionWorld[currentCascade].w;  

	for (uint i=0; i<MAX_STEPS; i++)
	{
		if(atm > 0.95)	break;

		vec3 pos = rayOrigin+stepped;

		//todo: feels like there must be a better way
		// Check if early out
		vec3 temp1 = sign(pos - vec3(minpos));
		vec3 temp2 = sign(vec3(maxpos) - pos);
		float inside = dot(temp1,temp2);
		// Check if still inside boundaries, and maximum transmittance is alowed
		if(inside < 3.0)			
		{
			if(currentCascade >= (ubo.cascadeCount-1)) break;

			currentCascade++;
			minpos = ubo.voxelRegionWorld[currentCascade].xyz;
			maxpos = minpos + ubo.voxelRegionWorld[currentCascade].w; 
		}

		// Calculate the voxelsize depending on cascade
		gvoxelSize = ubo.voxelRegionWorld[currentCascade].w / float(ubo.voxelGridResolution);

		dist = distance(pos, rayOrigin);

		// Current voxelregionworld depending on cascade
		vec4 CurrentvoxelRegionWorld = ubo.voxelRegionWorld[currentCascade];
		// The voxel position depending on the current cascade
		vec3 voxelPosition = (pos - CurrentvoxelRegionWorld.xyz)/CurrentvoxelRegionWorld.w;		// range from 0.0-1.0
		
		// Cacculate the aperture
		float aperture = max(coneTheta*dist,gvoxelSize);
		float mipLevel = log2(aperture/gvoxelSize);

		// Sample anisotropic
		vec4 res = SampleAnisotropic(voxelPosition, rd, currentCascade, mipLevel);

		// Weight Ao by distance f(r) = 1/(1+K*r);
		a = min((1/(1+scalar*dist*dist)), 1.0);
		//a = 1;

		// front-to-back accumulation
		// c = ac+(1-a)a2 p
		// a = a+(1-a) p
		acol = acol.rgb*atm+(1-atm)*res.rgb*res.a*a;
		atm = atm+(1-atm)*(res.a*a);

		// Take step relative to the interpolated size
		float stepSize = aperture * STEPSIZE_WRT_TEXEL;

		// Increment stepped value
		stepped += stepSize*rd;
    }
	// Weight Ao by distance f(r) = 1/(1+K*r);
	// Do attenuation here for now.
	//float at = min((1/(1+scalar*dist*dist)), 1.0);
	float at = 1;
	atm = atm*at;
	//acol.rgb = acol.rgb*min((1/(1+0.01*dist*dist)), 1.0);

    return vec4(acol.rgb, 1-atm);
}

void main() 
{
	// Read G-Buffer values from previous sub pass
	vec3 position = subpassLoad(samplerposition).rgb;
	vec3 normal = subpassLoad(samplerNormal).rgb;
	vec4 albedo = subpassLoad(samplerAlbedo);
	vec3 tangent = subpassLoad(samplerTangent).rgb;

	// Pixel world position
	vec3 worldpos = position;
	
	// Diffuse color
	vec3 diffuse = albedo.rgb;
	
	// Normals
	normal = normalize(normal);
	tangent = normalize(tangent);

	// Accumilated color
	vec3 acout = vec3(0);

	// find the dominant axis
	vec3 tocamera = abs(ubo.cameraPosition - worldpos);
	vec3 dominant = (tocamera.x >= tocamera.y) ? vec3(tocamera.x,0,0) : vec3(0,tocamera.y,0);
	dominant = dominant.x+dominant.y+dominant.z < tocamera.z ? vec3(0,0,tocamera.z) : dominant;
	dominant = sign(dominant);		// mask
	// calculate current cascade
	tocamera = tocamera * dominant;
	float dist = tocamera.x+tocamera.y+tocamera.z;

	gstartCascade = uint(ceil(max(log2(dist/(ubo.voxelRegionWorld[0].w*0.5)),0.0)));

	// Current voxelregionworld depending on cascade
	vec4 CurrentvoxelRegionWorld = ubo.voxelRegionWorld[gstartCascade];
	gvoxelSize = CurrentvoxelRegionWorld.w / float(ubo.voxelGridResolution);

	vec4 maxregion = ubo.voxelRegionWorld[ubo.cascadeCount-1];
	float maxaperture = maxregion.w/6.0;	//Maximum size of a voxel, and cone scalar...
	float l = maxregion.w; 

	//offset
	worldpos += normal*gvoxelSize*2;

	const float theta = atan((maxaperture)/l);
	vec4 indir = vec4(0.0);
	{
		const float theta = atan((maxaperture)/l);
        const float NORMAL_ROTATE = radians(45.0);
		const float conesPerFragment = ubo.conecount;
        const float ANGLE_ROTATE = 2.0*PI / conesPerFragment;
	
		//get a perpendicular from the normal in the XZ plane
        for (float i=0.0; i<conesPerFragment; i++)
		{
            vec3 rotatedAxis = Rotate(tangent, ANGLE_ROTATE*(i), normal);
			vec3 rd = normalize(Rotate(normal, NORMAL_ROTATE, rotatedAxis));
			indir += conetraceIndir(worldpos+rd*gvoxelSize, rd, theta, AO_DIST_K);	
        }
	
		indir += conetraceIndir(worldpos+normal*gvoxelSize, normal, theta, AO_DIST_K);	
		indir /= conesPerFragment+1;
    }
	
	//specular 
	tocamera = normalize(worldpos-ubo.cameraPosition);
	vec3 specularvec = normalize(reflect(tocamera,normal));
	vec4 specular = conetraceIndir(worldpos+specularvec*gvoxelSize, specularvec, theta, INDIR_DIST_K);		// voxel offset

	#define SPECULAR_SCALAR 2
	#define INDIR_SCALAR 2
	specular*= SPECULAR_SCALAR;
	indir.xyz*= INDIR_SCALAR;

	outColor = vec4(indir.xyz+specular.xyz,indir.a);
	imageStore(scaledOutput, ivec2(inUV.x * ubo.scaledWidth, inUV.y * ubo.scaledHeight), outColor);
}