/*
	OpenGEX Import Template Software License
	==========================================

	OpenGEX Import Template, version 1.1.2
	Copyright 2014-2015, Eric Lengyel
	All rights reserved.

	The OpenGEX Import Template is free software published on the following website:

		http://opengex.org/

	Redistribution and use in source and binary forms, with or without modification,
	are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the entire text of this license,
	comprising the above copyright notice, this list of conditions, and the following
	disclaimer.
	
	2. Redistributions of any modified source code files must contain a prominent
	notice immediately following this license stating that the contents have been
	modified from their original form.

	3. Redistributions in binary form must include attribution to the author in any
	listing of credits provided with the distribution. If there is no listing of
	credits, then attribution must be included in the documentation and/or other
	materials provided with the distribution. The attribution must be exactly the
	statement "This software contains an OpenGEX import module based on work by
	Eric Lengyel" (without quotes).

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "OpenGEX.h"

#include <windows.h>
#include <stdint.h>
#include <assert.h>

#include "AssetManager.h"
#include "MurmurHash.h"
#include "Defines.h"

using namespace OGEX;

#define DEFAULT_SEED 0xA86F13C7 

extern void LoadAssetStaticManager(char* path, uint32_t pathLenght);

enum upVector
{
	UP_VECTOR_X,
	UP_VECTOR_Y,
	UP_VECTOR_Z
};

enum
{
	MESH_PRIMITIVE_TYPE_TRIANGLE_LIST = 1
};

enum
{
	FLAG_HAS_POSITION = (1 << 0),
	FLAG_HAS_TEXCOORD = (1 << 1),
	FLAG_HAS_TANGENT = (1 << 2),
	FLAG_HAS_NORMAL = (1 << 3),
};

enum
{
	VERTEX_BUFFER_ELEMENT_TYPE_HALF,
	VERTEX_BUFFER_ELEMENT_TYPE_FLOAT,
	VERTEX_BUFFER_ELEMENT_TYPE_DOUBLE
};

struct OgexScanInfo
{
	uint32_t modelCount, meshCount, materialCount;
	uint32_t modelReferenceCount;
	uint32_t materialReferenceCount, textureReferenceCount;
	uint32_t pointLightCount, spotLightCount, directionalLightCount;

	uint32_t vertexArrayCount, indexArrayCount;
	uint64_t totalVertexByteCount, totalIndexByteCount;
	uint32_t flags;

	struct OgexStringTableEntry
	{
		uint64_t hash;
		union 
		{
			const char* strPtr;
			uint64_t offset;
		};
	} *stringTable;

	uint32_t stringTableSlots;

	struct ogesTextureTableEntry
	{
		uint64_t pathOffset;
		uint32_t textureIndex;
	}*textureTable;
	uint32_t textureTableSlots;
	uint32_t textureCount;

	//////////////////////////

	void AddString(const char* string)
	{
		uint64_t out[2];
		MurmurHash3_x64_128(string, (int)strlen(string), DEFAULT_SEED, out);

		uint64_t slot = out[1] % stringTableSlots;
		for (uint32_t i = 0; i < 32; i++)
		{
			if (stringTable[slot].strPtr == NULL)
			{
				stringTable[slot].strPtr = string;
				stringTable[slot].hash = out[0];
				return;
			}
			else if (stringTable[slot].hash == out[0])
			{
				assert(strcmp(string, stringTable[slot].strPtr) == 0);	// Make sure this is not a hash collision
				return;	// Already in the table; Dupe!
			}
			out[1] = Hash64Shift(out[1]);
			slot = out[1] % stringTableSlots;
		}
		assert(false); // Could not find an empty slot!
	}

	uint64_t FindStringOffset(const char* string)
	{
		uint64_t out[2];
		MurmurHash3_x64_128(string, (int)strlen(string), DEFAULT_SEED, out);

		uint64_t slot = out[1] % stringTableSlots;
		for (uint32_t i = 0; i < 32; i++)
		{
			assert(stringTable[slot].offset != 0xFFFFFFFFFFFFFFFFULL);
			if (stringTable[slot].hash == out[0])
			{
				return stringTable[slot].offset;
			}
			out[1] = Hash64Shift(out[1]);
			slot = out[1] % stringTableSlots;
		}
		assert(false); // Could not find an empty slot!
		return 0xFFFFFFFFFFFFFFFF;
	}

	//////////////////////////

	uint32_t AddTexture(uint64_t pathOffset)
	{
		uint64_t hash = pathOffset;
		uint64_t slot = hash % textureTableSlots;
		for (uint32_t i = 0; i < 32; i++)
		{
			if (textureTable[slot].pathOffset == 0xFFFFFFFFFFFFFFFF)
			{
				textureTable[slot].pathOffset = pathOffset;
				textureTable[slot].textureIndex = textureCount++;
				return textureTable[slot].textureIndex;
			}
			else if (textureTable[slot].pathOffset == pathOffset)
			{
				return textureTable[slot].textureIndex;	// Already in the table; Dupe!
			}
			hash = Hash64Shift(hash);
			slot = hash % textureTableSlots;
		}
		assert(false); // Could not find an empty slot!
		return 0xFFFFFFFF;
	}

	uint32_t FindTexture(uint64_t pathOffset)
	{
		uint64_t hash = pathOffset;
		uint64_t slot = hash % textureTableSlots;
		for (uint32_t i = 0; i < 32; i++)
		{
			if (textureTable[slot].pathOffset == pathOffset)
			{
				return textureTable[slot].textureIndex;
			}
			hash = Hash64Shift(hash);
			slot = hash % textureTableSlots;
		}
		assert(false); // Could not find an empty slot!
		return 0xFFFFFFFF;
	}
};

OgexScanInfo* ScanInfo;

OpenGexStructure::OpenGexStructure(StructureType type) : Structure(type)
{
}

OpenGexStructure::~OpenGexStructure()
{
}

Structure *OpenGexStructure::GetFirstCoreSubnode(void) const
{
	Structure *structure = GetFirstSubnode();
	while ((structure) && (structure->GetStructureType() == kStructureExtension))
	{
		structure = structure->Next();
	}

	return (structure);
}

Structure *OpenGexStructure::GetLastCoreSubnode(void) const
{
	Structure *structure = GetLastSubnode();
	while ((structure) && (structure->GetStructureType() == kStructureExtension))
	{
		structure = structure->Previous();
	}

	return (structure);
}

bool OpenGexStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	return (structure->GetStructureType() == kStructureExtension);
}


MetricStructure::MetricStructure() : OpenGexStructure(kStructureMetric)
{
}

MetricStructure::~MetricStructure()
{
}

bool MetricStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "key")
	{
		*type = kDataString;
		*value = &metricKey;
		return (true);
	}

	return (false);
}

bool MetricStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetBaseStructureType() == kStructurePrimitive)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult MetricStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	if (metricKey == "distance")
	{
		if (structure->GetStructureType() != kDataFloat)
		{
			return (kDataInvalidDataFormat);
		}

		const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
		if (dataStructure->GetDataElementCount() != 1)
		{
			return (kDataInvalidDataFormat);
		}

		static_cast<OpenGexDataDescription *>(dataDescription)->SetDistanceScale(dataStructure->GetDataElement(0));
	}
	else if (metricKey == "angle")
	{
		if (structure->GetStructureType() != kDataFloat)
		{
			return (kDataInvalidDataFormat);
		}

		const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
		if (dataStructure->GetDataElementCount() != 1)
		{
			return (kDataInvalidDataFormat);
		}

		static_cast<OpenGexDataDescription *>(dataDescription)->SetAngleScale(dataStructure->GetDataElement(0));
	}
	else if (metricKey == "time")
	{
		if (structure->GetStructureType() != kDataFloat)
		{
			return (kDataInvalidDataFormat);
		}

		const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
		if (dataStructure->GetDataElementCount() != 1)
		{
			return (kDataInvalidDataFormat);
		}

		static_cast<OpenGexDataDescription *>(dataDescription)->SetTimeScale(dataStructure->GetDataElement(0));
	}
	else if (metricKey == "up")
	{
		int32	direction;

		if (structure->GetStructureType() != kDataString)
		{
			return (kDataInvalidDataFormat);
		}

		const DataStructure<StringDataType> *dataStructure = static_cast<const DataStructure<StringDataType> *>(structure);
		if (dataStructure->GetDataElementCount() != 1)
		{
			return (kDataInvalidDataFormat);
		}

		const String& string = dataStructure->GetDataElement(0);
		if (string == "z")
		{
			direction = 2;
		}
		else if (string == "y")
		{
			direction = 1;
		}
		else
		{
			return (kDataOpenGexInvalidUpDirection);
		}

		static_cast<OpenGexDataDescription *>(dataDescription)->SetUpDirection(direction);
	}

	return (kDataOkay);
}


NameStructure::NameStructure() : OpenGexStructure(kStructureName)
{
}

NameStructure::~NameStructure()
{
}

bool NameStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataString)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult NameStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<StringDataType> *dataStructure = static_cast<const DataStructure<StringDataType> *>(structure);
	if (dataStructure->GetDataElementCount() != 1)
	{
		return (kDataInvalidDataFormat);
	}

	name = dataStructure->GetDataElement(0);
	return (kDataOkay);
}


ObjectRefStructure::ObjectRefStructure() : OpenGexStructure(kStructureObjectRef)
{
	targetStructure = nullptr;
}

ObjectRefStructure::~ObjectRefStructure()
{
}

bool ObjectRefStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataRef)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult ObjectRefStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<RefDataType> *dataStructure = static_cast<const DataStructure<RefDataType> *>(structure);
	if (dataStructure->GetDataElementCount() != 0)
	{
		Structure *objectStructure = dataDescription->FindStructure(dataStructure->GetDataElement(0));
		if (objectStructure)
		{
			targetStructure = objectStructure;
			return (kDataOkay);
		}
	}

	return (kDataBrokenRef);
}


MaterialRefStructure::MaterialRefStructure() : OpenGexStructure(kStructureMaterialRef)
{
	materialIndex = 0;
	targetStructure = nullptr;
}

MaterialRefStructure::~MaterialRefStructure()
{
}

bool MaterialRefStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "index")
	{
		*type = kDataUnsignedInt32;
		*value = &materialIndex;
		return (true);
	}

	return (false);
}

bool MaterialRefStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataRef)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult MaterialRefStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<RefDataType> *dataStructure = static_cast<const DataStructure<RefDataType> *>(structure);
	if (dataStructure->GetDataElementCount() != 0)
	{
		const Structure *materialStructure = dataDescription->FindStructure(dataStructure->GetDataElement(0));
		if (materialStructure)
		{
			if (materialStructure->GetStructureType() != kStructureMaterial)
			{
				return (kDataOpenGexInvalidMaterialRef);
			}

			targetStructure = static_cast<const MaterialStructure *>(materialStructure);
			return (kDataOkay);
		}
	}

	return (kDataBrokenRef);
}


AnimatableStructure::AnimatableStructure(StructureType type) : OpenGexStructure(type)
{
}

AnimatableStructure::~AnimatableStructure()
{
}


MatrixStructure::MatrixStructure(StructureType type) : AnimatableStructure(type)
{
	SetBaseStructureType(kStructureMatrix);

	objectFlag = false;
}

MatrixStructure::~MatrixStructure()
{
}

bool MatrixStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "object")
	{
		*type = kDataBool;
		*value = &objectFlag;
		return (true);
	}

	return (false);
}


TransformStructure::TransformStructure() : MatrixStructure(kStructureTransform)
{
}

TransformStructure::~TransformStructure()
{
}

bool TransformStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
		return (primitiveStructure->GetArraySize() == 16);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult TransformStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);

	transformCount = dataStructure->GetDataElementCount() / 16;
	if (transformCount == 0)
	{
		return (kDataInvalidDataFormat);
	}

	transformArray = &dataStructure->GetDataElement(0);
	return (kDataOkay);
}


TranslationStructure::TranslationStructure() :
		MatrixStructure(kStructureTranslation),
		translationKind("xyz")
{
}

TranslationStructure::~TranslationStructure()
{
}

bool TranslationStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "kind")
	{
		*type = kDataString;
		*value = &translationKind;
		return (true);
	}

	return (false);
}

bool TranslationStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
		unsigned_int32 arraySize = primitiveStructure->GetArraySize();
		return ((arraySize == 0) || (arraySize == 3));
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult TranslationStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	unsigned_int32 arraySize = dataStructure->GetArraySize();

	if ((translationKind == "x") || (translationKind == "y") || (translationKind == "z"))
	{
		if ((arraySize != 0) || (dataStructure->GetDataElementCount() != 1))
		{
			return (kDataInvalidDataFormat);
		}
	}
	else if (translationKind == "xyz")
	{
		if ((arraySize != 3) || (dataStructure->GetDataElementCount() != 3))
		{
			return (kDataInvalidDataFormat);
		}
	}
	else
	{
		return (kDataOpenGexInvalidTranslationKind);
	}

	const float *data = &dataStructure->GetDataElement(0);

	// Data is 1 or 3 floats depending on kind.
	// Build application-specific transform here.

	return (kDataOkay);
}


RotationStructure::RotationStructure() :
		MatrixStructure(kStructureRotation),
		rotationKind("axis")
{
}

RotationStructure::~RotationStructure()
{
}

bool RotationStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "kind")
	{
		*type = kDataString;
		*value = &rotationKind;
		return (true);
	}

	return (false);
}

bool RotationStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
		unsigned_int32 arraySize = primitiveStructure->GetArraySize();
		return ((arraySize == 0) || (arraySize == 4));
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult RotationStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	unsigned_int32 arraySize = dataStructure->GetArraySize();

	if ((rotationKind == "x") || (rotationKind == "y") || (rotationKind == "z"))
	{
		if ((arraySize != 0) || (dataStructure->GetDataElementCount() != 1))
		{
			return (kDataInvalidDataFormat);
		}
	}
	else if ((rotationKind == "axis") || (rotationKind == "quaternion"))
	{
		if ((arraySize != 4) || (dataStructure->GetDataElementCount() != 4))
		{
			return (kDataInvalidDataFormat);
		}
	}
	else
	{
		return (kDataOpenGexInvalidRotationKind);
	}

	const float *data = &dataStructure->GetDataElement(0);

	// Data is 1 or 4 floats depending on kind.
	// Build application-specific transform here.

	return (kDataOkay);
}


ScaleStructure::ScaleStructure() :
		MatrixStructure(kStructureScale),
		scaleKind("xyz")
{
}

ScaleStructure::~ScaleStructure()
{
}

bool ScaleStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "kind")
	{
		*type = kDataString;
		*value = &scaleKind;
		return (true);
	}

	return (false);
}

bool ScaleStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
		unsigned_int32 arraySize = primitiveStructure->GetArraySize();
		return ((arraySize == 0) || (arraySize == 3));
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult ScaleStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	unsigned_int32 arraySize = dataStructure->GetArraySize();

	if ((scaleKind == "x") || (scaleKind == "y") || (scaleKind == "z"))
	{
		if ((arraySize != 0) || (dataStructure->GetDataElementCount() != 1))
		{
			return (kDataInvalidDataFormat);
		}
	}
	else if (scaleKind == "xyz")
	{
		if ((arraySize != 3) || (dataStructure->GetDataElementCount() != 3))
		{
			return (kDataInvalidDataFormat);
		}
	}
	else
	{
		return (kDataOpenGexInvalidScaleKind);
	}

	const float *data = &dataStructure->GetDataElement(0);

	// Data is 1 or 3 floats depending on kind.
	// Build application-specific transform here.

	return (kDataOkay);
}


MorphWeightStructure::MorphWeightStructure() : AnimatableStructure(kStructureMorphWeight)
{
	morphIndex = 0;
	morphWeight = 0.0F;
}

MorphWeightStructure::~MorphWeightStructure()
{
}

bool MorphWeightStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "index")
	{
		*type = kDataUnsignedInt32;
		*value = &morphIndex;
		return (true);
	}

	return (false);
}

bool MorphWeightStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
		unsigned_int32 arraySize = dataStructure->GetArraySize();
		return (arraySize == 0);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult MorphWeightStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	if (dataStructure->GetDataElementCount() == 1)
	{
		morphWeight = dataStructure->GetDataElement(0);
	}
	else
	{
		return (kDataInvalidDataFormat);
	}

	// Do application-specific morph weight processing here.

	return (kDataOkay);
}


NodeStructure::NodeStructure() : OpenGexStructure(kStructureNode)
{
	SetBaseStructureType(kStructureNode);
}

NodeStructure::NodeStructure(StructureType type) : OpenGexStructure(type)
{
	SetBaseStructureType(kStructureNode);
}

NodeStructure::~NodeStructure()
{
}

bool NodeStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetBaseStructureType();
	if ((type == kStructureNode) || (type == kStructureMatrix))
	{
		return (true);
	}

	type = structure->GetStructureType();
	if ((type == kStructureName) || (type == kStructureAnimation))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult NodeStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const Structure *structure = GetFirstSubstructure(kStructureName);
	if (structure)
	{
		if (GetLastSubstructure(kStructureName) != structure)
		{
			return (kDataExtraneousSubstructure);
		}

		nodeName = static_cast<const NameStructure *>(structure)->GetName();
		//edit
		ScanInfo->AddString(nodeName);
	}
	else
	{
		nodeName = nullptr;
	}

	// Do application-specific node processing here.

	structure = GetFirstSubnode();
	while (structure)
	{
		switch (structure->GetStructureType())
		{
		case OGEX::kStructureTranslation:
		case OGEX::kStructureRotation:
		case OGEX::kStructureScale:
			assert(0);
			break;

		case OGEX::kStructureTransform:
			const TransformStructure* transformStructure = static_cast<const TransformStructure*> (structure);
			const float* transformArr = transformStructure->GetTransform(0);
			memcpy((void*)&localTransform[0], transformArr, 16 * sizeof(float));

			break;
		}
		structure = structure->Next();
	}

	return (kDataOkay);
}

const ObjectStructure *NodeStructure::GetObjectStructure(void) const
{
	return (nullptr);
}


BoneNodeStructure::BoneNodeStructure() : NodeStructure(kStructureBoneNode)
{
}

BoneNodeStructure::~BoneNodeStructure()
{
}


GeometryNodeStructure::GeometryNodeStructure() : NodeStructure(kStructureGeometryNode)
{
	// The first entry in each of the following arrays indicates whether the flag
	// is specified by a property. If true, then the second entry in the array
	// indicates the actual value that the property specified.

	visibleFlag[0] = false;
	shadowFlag[0] = false;
	motionBlurFlag[0] = false;
}

GeometryNodeStructure::~GeometryNodeStructure()
{
}

bool GeometryNodeStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "visible")
	{
		*type = kDataBool;
		*value = &visibleFlag[1];
		visibleFlag[0] = true;
		return (true);
	}

	if (identifier == "shadow")
	{
		*type = kDataBool;
		*value = &shadowFlag[1];
		shadowFlag[0] = true;
		return (true);
	}

	if (identifier == "motion_blur")
	{
		*type = kDataBool;
		*value = &motionBlurFlag[1];
		motionBlurFlag[0] = true;
		return (true);
	}

	return (false);
}

bool GeometryNodeStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kStructureObjectRef) || (type == kStructureMaterialRef) || (type == kStructureMorphWeight))
	{
		return (true);
	}

	return (NodeStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult GeometryNodeStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = NodeStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	bool objectFlag = false;
	bool materialFlag[256] = {false};
	int32 maxMaterialIndex = -1;

	Structure *structure = GetFirstSubnode();
	while (structure)
	{
		StructureType type = structure->GetStructureType();
		if (type == kStructureObjectRef)
		{
			if (objectFlag)
			{
				return (kDataExtraneousSubstructure);
			}

			objectFlag = true;

			Structure *objectStructure = static_cast<ObjectRefStructure *>(structure)->GetTargetStructure();
			if (objectStructure->GetStructureType() != kStructureGeometryObject)
			{
				return (kDataOpenGexInvalidObjectRef);
			}

			geometryObjectStructure = static_cast<GeometryObjectStructure *>(objectStructure);
		}
		else if (type == kStructureMaterialRef)
		{
			const MaterialRefStructure *materialRefStructure = static_cast<MaterialRefStructure *>(structure);

			unsigned_int32 index = materialRefStructure->GetMaterialIndex();
			if (index > 255)
			{
				// We only support up to 256 materials.
				return (kDataOpenGexMaterialIndexUnsupported);
			}

			if (materialFlag[index])
			{
				return (kDataOpenGexDuplicateMaterialRef);
			}

			materialFlag[index] = true;
			maxMaterialIndex = Max(maxMaterialIndex, index);
		}

		structure = structure->Next();
	}

	if (!objectFlag)
	{
		return (kDataMissingSubstructure);
	}

	if (maxMaterialIndex >= 0)
	{
		for (machine a = 0; a <= maxMaterialIndex; a++)
		{
			if (!materialFlag[a])
			{
				return (kDataOpenGexMissingMaterialRef);
			}
		}

		materialStructureArray.SetElementCount(maxMaterialIndex + 1);

		structure = GetFirstSubnode();
		while (structure)
		{
			if (structure->GetStructureType() == kStructureMaterialRef)
			{
				const MaterialRefStructure *materialRefStructure = static_cast<const MaterialRefStructure *>(structure);
				materialStructureArray[materialRefStructure->GetMaterialIndex()] = materialRefStructure->GetTargetStructure();
			}

			structure = structure->Next();
		}
	}

	// Do application-specific node processing here.
	//edit
	ScanInfo->modelReferenceCount++;
	ScanInfo->materialReferenceCount += maxMaterialIndex + 1;

	return (kDataOkay);
}

const ObjectStructure *GeometryNodeStructure::GetObjectStructure(void) const
{
	return (geometryObjectStructure);
}


LightNodeStructure::LightNodeStructure() : NodeStructure(kStructureLightNode)
{
	// The first entry in the following array indicates whether the flag is
	// specified by a property. If true, then the second entry in the array
	// indicates the actual value that the property specified.

	shadowFlag[0] = false;
}

LightNodeStructure::~LightNodeStructure()
{
}

bool LightNodeStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "shadow")
	{
		*type = kDataBool;
		*value = &shadowFlag[1];
		shadowFlag[0] = true;
		return (true);
	}

	return (false);
}

bool LightNodeStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureObjectRef)
	{
		return (true);
	}

	return (NodeStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult LightNodeStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = NodeStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	bool objectFlag = false;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kStructureObjectRef)
		{
			if (objectFlag)
			{
				return (kDataExtraneousSubstructure);
			}

			objectFlag = true;

			const Structure *objectStructure = static_cast<const ObjectRefStructure *>(structure)->GetTargetStructure();
			if (objectStructure->GetStructureType() != kStructureLightObject)
			{
				return (kDataOpenGexInvalidObjectRef);
			}

			lightObjectStructure = static_cast<const LightObjectStructure *>(objectStructure);
		}

		structure = structure->Next();
	}

	if (!objectFlag)
	{
		return (kDataMissingSubstructure);
	}

	// Do application-specific node processing here.

	return (kDataOkay);
}

const ObjectStructure *LightNodeStructure::GetObjectStructure(void) const
{
	return (lightObjectStructure);
}


CameraNodeStructure::CameraNodeStructure() : NodeStructure(kStructureCameraNode)
{
}

CameraNodeStructure::~CameraNodeStructure()
{
}

bool CameraNodeStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureObjectRef)
	{
		return (true);
	}

	return (NodeStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult CameraNodeStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = NodeStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	bool objectFlag = false;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kStructureObjectRef)
		{
			if (objectFlag)
			{
				return (kDataExtraneousSubstructure);
			}

			objectFlag = true;

			const Structure *objectStructure = static_cast<const ObjectRefStructure *>(structure)->GetTargetStructure();
			if (objectStructure->GetStructureType() != kStructureCameraObject)
			{
				return (kDataOpenGexInvalidObjectRef);
			}

			cameraObjectStructure = static_cast<const CameraObjectStructure *>(objectStructure);
		}

		structure = structure->Next();
	}

	if (!objectFlag)
	{
		return (kDataMissingSubstructure);
	}

	// Do application-specific node processing here.

	return (kDataOkay);
}

const ObjectStructure *CameraNodeStructure::GetObjectStructure(void) const
{
	return (cameraObjectStructure);
}


VertexArrayStructure::VertexArrayStructure() : OpenGexStructure(kStructureVertexArray)
{
	morphIndex = 0;
}

VertexArrayStructure::~VertexArrayStructure()
{
}

bool VertexArrayStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "attrib")
	{
		*type = kDataString;
		*value = &arrayAttrib;
		return (true);
	}

	if (identifier == "morph")
	{
		*type = kDataUnsignedInt32;
		*value = &morphIndex;
		return (true);
	}

	return (false);
}

bool VertexArrayStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult VertexArrayStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	//edit
	// Do something with the vertex data here.
	vertexArrayIndex = ScanInfo->vertexArrayCount++;

	const ODDL::Structure* dataStructure = GetFirstSubnode();
	switch (dataStructure->GetStructureType())
	{
	case ODDL::kDataHalf:
		totalByteSize = 
			sizeof(ODDL::HalfDataType::PrimType) 
			* static_cast<const ODDL::DataStructure<ODDL::HalfDataType>*>(dataStructure)->GetDataElementCount();
		vertexCount =
			static_cast<const ODDL::DataStructure<ODDL::HalfDataType>*> (dataStructure)->GetDataElementCount()
			/ static_cast<const ODDL::DataStructure<ODDL::HalfDataType>*> (dataStructure)->GetArraySize();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::HalfDataType>*> (dataStructure)->GetDataElement(0);
		elementCount = static_cast<const ODDL::DataStructure<ODDL::HalfDataType>*> (dataStructure)->GetArraySize();
		elementType = VERTEX_BUFFER_ELEMENT_TYPE_HALF;
		if (vertexCount == 0)
			return (kDataOpenGexVertexCountUnsupported);
		break;
	case ODDL::kDataFloat:
		totalByteSize =
			sizeof(ODDL::FloatDataType::PrimType)
			* static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> (dataStructure)->GetDataElementCount();
		vertexCount =
			static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> (dataStructure)->GetDataElementCount()
			/ static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> (dataStructure)->GetArraySize();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> (dataStructure)->GetDataElement(0);
		elementCount = static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> (dataStructure)->GetArraySize();
		elementType = VERTEX_BUFFER_ELEMENT_TYPE_FLOAT;
		if (vertexCount == 0)
			return (kDataOpenGexVertexCountUnsupported);
		break;
	case ODDL::kDataDouble:
		totalByteSize =
			sizeof(ODDL::DoubleDataType::PrimType)
			* static_cast<const ODDL::DataStructure<ODDL::DoubleDataType>*> (dataStructure)->GetDataElementCount();
		vertexCount =
			static_cast<const ODDL::DataStructure<ODDL::DoubleDataType>*> (dataStructure)->GetDataElementCount()
			/ static_cast<const ODDL::DataStructure<ODDL::DoubleDataType>*> (dataStructure)->GetArraySize();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::DoubleDataType>*> (dataStructure)->GetDataElement(0);
		elementCount = static_cast<const ODDL::DataStructure<ODDL::DoubleDataType>*> (dataStructure)->GetArraySize();
		elementType = VERTEX_BUFFER_ELEMENT_TYPE_DOUBLE;
		if (vertexCount == 0)
			return (kDataOpenGexVertexCountUnsupported);
		break;
	default:
		return (kDataExtraneousSubstructure);
	}
	ScanInfo->totalVertexByteCount += totalByteSize;

	return (kDataOkay);
}


IndexArrayStructure::IndexArrayStructure() : OpenGexStructure(kStructureIndexArray)
{
	materialIndex = 0;
	restartIndex = 0;
	frontFace = "ccw";
}

IndexArrayStructure::~IndexArrayStructure()
{
}

bool IndexArrayStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "material")
	{
		*type = kDataUnsignedInt32;
		*value = &materialIndex;
		return (true);
	}

	if (identifier == "restart")
	{
		*type = kDataUnsignedInt64;
		*value = &restartIndex;
		return (true);
	}

	if (identifier == "front")
	{
		*type = kDataString;
		*value = &frontFace;
		return (true);
	}

	return (false);
}

bool IndexArrayStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kDataUnsignedInt8) || (type == kDataUnsignedInt16) || (type == kDataUnsignedInt32) || (type == kDataUnsignedInt64))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult IndexArrayStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
	if (primitiveStructure->GetArraySize() != 3)
	{
		return (kDataInvalidDataFormat);
	}

	//edit
	// Do something with the index array here.
	indexBufferIndex = ScanInfo->indexArrayCount++;
	const ODDL::Structure* dataStructure = GetFirstSubnode();

	switch (dataStructure->GetStructureType())
	{
	case ODDL::kDataUnsignedInt8:
		indexSize = sizeof(ODDL::UnsignedInt8DataType::PrimType);
		indexCount = static_cast<const ODDL::DataStructure<ODDL::UnsignedInt8DataType>*> (dataStructure)->GetDataElementCount();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::UnsignedInt8DataType>*> (dataStructure)->GetDataElement(0);
		break;
	case ODDL::kDataUnsignedInt16:
		indexSize = sizeof(ODDL::UnsignedInt16DataType::PrimType);
		indexCount = static_cast<const ODDL::DataStructure<ODDL::UnsignedInt16DataType>*> (dataStructure)->GetDataElementCount();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::UnsignedInt16DataType>*> (dataStructure)->GetDataElement(0);
		break;
	case ODDL::kDataUnsignedInt32:
		indexSize = sizeof(ODDL::UnsignedInt32DataType::PrimType);
		indexCount = static_cast<const ODDL::DataStructure<ODDL::UnsignedInt32DataType>*> (dataStructure)->GetDataElementCount();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::UnsignedInt32DataType>*> (dataStructure)->GetDataElement(0);
		break;
	case ODDL::kDataUnsignedInt64:
		indexSize = sizeof(ODDL::UnsignedInt64DataType::PrimType);
		indexCount = static_cast<const ODDL::DataStructure<ODDL::UnsignedInt64DataType>*> (dataStructure)->GetDataElementCount();
		data = (void*)&static_cast<const ODDL::DataStructure<ODDL::UnsignedInt64DataType>*> (dataStructure)->GetDataElement(0);
		break;
	default:
		return (kDataExtraneousSubstructure);
	}
	totalByteCount += indexSize * indexCount;
	ScanInfo->totalIndexByteCount += totalByteCount;

	return (kDataOkay);
}


BoneRefArrayStructure::BoneRefArrayStructure() : OpenGexStructure(kStructureBoneRefArray)
{
	boneNodeArray = nullptr;
}

BoneRefArrayStructure::~BoneRefArrayStructure()
{
	delete[] boneNodeArray;
}

bool BoneRefArrayStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataRef)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult BoneRefArrayStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<RefDataType> *dataStructure = static_cast<const DataStructure<RefDataType> *>(structure);
	boneCount = dataStructure->GetDataElementCount();

	if (boneCount != 0)
	{
		boneNodeArray = new const BoneNodeStructure *[boneCount];

		for (machine a = 0; a < boneCount; a++)
		{
			const StructureRef& reference = dataStructure->GetDataElement((uint32_t)a);
			const Structure *boneStructure = dataDescription->FindStructure(reference);
			if (!boneStructure)
			{
				return (kDataBrokenRef);
			}

			if (boneStructure->GetStructureType() != kStructureBoneNode)
			{
				return (kDataOpenGexInvalidBoneRef);
			}

			boneNodeArray[a] = static_cast<const BoneNodeStructure *>(boneStructure);
		}
	}

	return (kDataOkay);
}


BoneCountArrayStructure::BoneCountArrayStructure() : OpenGexStructure(kStructureBoneCountArray)
{
	arrayStorage = nullptr;
}

BoneCountArrayStructure::~BoneCountArrayStructure()
{
	delete[] arrayStorage;
}

bool BoneCountArrayStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kDataUnsignedInt8) || (type == kDataUnsignedInt16) || (type == kDataUnsignedInt32) || (type == kDataUnsignedInt64))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult BoneCountArrayStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
	if (primitiveStructure->GetArraySize() != 0)
	{
		return (kDataInvalidDataFormat);
	}

	StructureType type = primitiveStructure->GetStructureType();
	if (type == kDataUnsignedInt16)
	{
		const DataStructure<UnsignedInt16DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt16DataType> *>(primitiveStructure);
		vertexCount = dataStructure->GetDataElementCount();
		boneCountArray = &dataStructure->GetDataElement(0);
	}
	else if (type == kDataUnsignedInt8)
	{
		const DataStructure<UnsignedInt8DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt8DataType> *>(primitiveStructure);
		vertexCount = dataStructure->GetDataElementCount();

		const unsigned_int8 *data = &dataStructure->GetDataElement(0);
		arrayStorage = new unsigned_int16[vertexCount];
		boneCountArray = arrayStorage;

		for (machine a = 0; a < vertexCount; a++)
		{
			arrayStorage[a] = data[a];
		}
	}
	else if (type == kDataUnsignedInt32)
	{
		const DataStructure<UnsignedInt32DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt32DataType> *>(primitiveStructure);
		vertexCount = dataStructure->GetDataElementCount();

		const unsigned_int32 *data = &dataStructure->GetDataElement(0);
		arrayStorage = new unsigned_int16[vertexCount];
		boneCountArray = arrayStorage;

		for (machine a = 0; a < vertexCount; a++)
		{
			unsigned_int32 index = data[a];
			if (index > 65535)
			{
				// We only support 16-bit counts or smaller.
				return (kDataOpenGexIndexValueUnsupported);
			}

			arrayStorage[a] = (unsigned_int16) index;
		}
	}
	else // must be 64-bit
	{
		const DataStructure<UnsignedInt64DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt64DataType> *>(primitiveStructure);
		vertexCount = dataStructure->GetDataElementCount();

		const unsigned_int64 *data = &dataStructure->GetDataElement(0);
		arrayStorage = new unsigned_int16[vertexCount];
		boneCountArray = arrayStorage;

		for (machine a = 0; a < vertexCount; a++)
		{
			unsigned_int64 index = data[a];
			if (index > 65535)
			{
				// We only support 16-bit counts or smaller.
				return (kDataOpenGexIndexValueUnsupported);
			}

			arrayStorage[a] = (unsigned_int16) index;
		}
	}

	return (kDataOkay);
}


BoneIndexArrayStructure::BoneIndexArrayStructure() : OpenGexStructure(kStructureBoneIndexArray)
{
	arrayStorage = nullptr;
}

BoneIndexArrayStructure::~BoneIndexArrayStructure()
{
	delete[] arrayStorage;
}

bool BoneIndexArrayStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kDataUnsignedInt8) || (type == kDataUnsignedInt16) || (type == kDataUnsignedInt32) || (type == kDataUnsignedInt64))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult BoneIndexArrayStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const PrimitiveStructure *primitiveStructure = static_cast<const PrimitiveStructure *>(structure);
	if (primitiveStructure->GetArraySize() != 0)
	{
		return (kDataInvalidDataFormat);
	}

	StructureType type = primitiveStructure->GetStructureType();
	if (type == kDataUnsignedInt16)
	{
		const DataStructure<UnsignedInt16DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt16DataType> *>(primitiveStructure);
		boneIndexCount = dataStructure->GetDataElementCount();
		boneIndexArray = &dataStructure->GetDataElement(0);
	}
	else if (type == kDataUnsignedInt8)
	{
		const DataStructure<UnsignedInt8DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt8DataType> *>(primitiveStructure);
		boneIndexCount = dataStructure->GetDataElementCount();

		const unsigned_int8 *data = &dataStructure->GetDataElement(0);
		arrayStorage = new unsigned_int16[boneIndexCount];
		boneIndexArray = arrayStorage;

		for (machine a = 0; a < boneIndexCount; a++)
		{
			arrayStorage[a] = data[a];
		}
	}
	else if (type == kDataUnsignedInt32)
	{
		const DataStructure<UnsignedInt32DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt32DataType> *>(primitiveStructure);
		boneIndexCount = dataStructure->GetDataElementCount();

		const unsigned_int32 *data = &dataStructure->GetDataElement(0);
		arrayStorage = new unsigned_int16[boneIndexCount];
		boneIndexArray = arrayStorage;

		for (machine a = 0; a < boneIndexCount; a++)
		{
			unsigned_int32 index = data[a];
			if (index > 65535)
			{
				// We only support 16-bit indexes or smaller.
				return (kDataOpenGexIndexValueUnsupported);
			}

			arrayStorage[a] = (unsigned_int16) index;
		}
	}
	else // must be 64-bit
	{
		const DataStructure<UnsignedInt64DataType> *dataStructure = static_cast<const DataStructure<UnsignedInt64DataType> *>(primitiveStructure);
		boneIndexCount = dataStructure->GetDataElementCount();

		const unsigned_int64 *data = &dataStructure->GetDataElement(0);
		arrayStorage = new unsigned_int16[boneIndexCount];
		boneIndexArray = arrayStorage;

		for (machine a = 0; a < boneIndexCount; a++)
		{
			unsigned_int64 index = data[a];
			if (index > 65535)
			{
				// We only support 16-bit indexes or smaller.
				return (kDataOpenGexIndexValueUnsupported);
			}

			arrayStorage[a] = (unsigned_int16) index;
		}
	}

	return (kDataOkay);
}


BoneWeightArrayStructure::BoneWeightArrayStructure() : OpenGexStructure(kStructureBoneWeightArray)
{
}

BoneWeightArrayStructure::~BoneWeightArrayStructure()
{
}

bool BoneWeightArrayStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult BoneWeightArrayStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	if (dataStructure->GetArraySize() != 0)
	{
		return (kDataInvalidDataFormat);
	}

	boneWeightCount = dataStructure->GetDataElementCount();
	boneWeightArray = &dataStructure->GetDataElement(0);
	return (kDataOkay);
}


SkeletonStructure::SkeletonStructure() : OpenGexStructure(kStructureSkeleton)
{
}

SkeletonStructure::~SkeletonStructure()
{
}

bool SkeletonStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kStructureBoneRefArray) || (type == kStructureTransform))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult SkeletonStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const Structure *structure = GetFirstSubstructure(kStructureBoneRefArray);
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastSubstructure(kStructureBoneRefArray) != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	boneRefArrayStructure = static_cast<const BoneRefArrayStructure *>(structure);

	structure = GetFirstSubstructure(kStructureTransform);
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastSubstructure(kStructureTransform) != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	transformStructure = static_cast<const TransformStructure *>(structure);

	if (boneRefArrayStructure->GetBoneCount() != transformStructure->GetTransformCount())
	{
		return (kDataOpenGexBoneCountMismatch);
	}

	return (kDataOkay);
}


SkinStructure::SkinStructure() : OpenGexStructure(kStructureSkin)
{
}

SkinStructure::~SkinStructure()
{
}

bool SkinStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kStructureTransform) || (type == kStructureSkeleton) || (type == kStructureBoneCountArray) || (type == kStructureBoneIndexArray) || (type == kStructureBoneWeightArray))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult SkinStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const Structure *structure = GetFirstSubstructure(kStructureTransform);
	if (structure)
	{
		if (GetLastSubstructure(kStructureTransform) != structure)
		{
			return (kDataExtraneousSubstructure);
		}

		// Process skin transform here.
	}

	structure = GetFirstSubstructure(kStructureSkeleton);
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastSubstructure(kStructureSkeleton) != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	skeletonStructure = static_cast<const SkeletonStructure *>(structure);

	structure = GetFirstSubstructure(kStructureBoneCountArray);
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastSubstructure(kStructureBoneCountArray) != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	boneCountArrayStructure = static_cast<const BoneCountArrayStructure *>(structure);

	structure = GetFirstSubstructure(kStructureBoneIndexArray);
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastSubstructure(kStructureBoneIndexArray) != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	boneIndexArrayStructure = static_cast<const BoneIndexArrayStructure *>(structure);

	structure = GetFirstSubstructure(kStructureBoneWeightArray);
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastSubstructure(kStructureBoneWeightArray) != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	boneWeightArrayStructure = static_cast<const BoneWeightArrayStructure *>(structure);

	int32 boneIndexCount = boneIndexArrayStructure->GetBoneIndexCount();
	if (boneWeightArrayStructure->GetBoneWeightCount() != boneIndexCount)
	{
		return (kDataOpenGexBoneWeightCountMismatch);
	}

	int32 vertexCount = boneCountArrayStructure->GetVertexCount();
	const unsigned_int16 *boneCountArray = boneCountArrayStructure->GetBoneCountArray();

	int32 boneWeightCount = 0;
	for (machine a = 0; a < vertexCount; a++)
	{
		unsigned_int32 count = boneCountArray[a];
		boneWeightCount += count;
	}

	if (boneWeightCount != boneIndexCount)
	{
		return (kDataOpenGexBoneWeightCountMismatch);
	}

	// Do application-specific skin processing here.

	return (kDataOkay);
}


MorphStructure::MorphStructure() : OpenGexStructure(kStructureMorph)
{
	// The value of baseFlag indicates whether the base property was actually
	// specified for the structure.

	morphIndex = 0;
	baseFlag = false;
}

MorphStructure::~MorphStructure()
{
}

bool MorphStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "index")
	{
		*type = kDataUnsignedInt32;
		*value = &morphIndex;
		return (true);
	}

	if (identifier == "base")
	{
		*type = kDataUnsignedInt32;
		*value = &baseIndex;
		baseFlag = true;
		return (true);
	}

	return (false);
}

bool MorphStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureName)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult MorphStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = OpenGexStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	morphName = nullptr;

	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	morphName = static_cast<const NameStructure *>(structure)->GetName();

	// Do application-specific morph processing here.

	return (kDataOkay);
}


MeshStructure::MeshStructure() : OpenGexStructure(kStructureMesh)
{
	meshLevel = 0;

	//edit
	vertexCount = 0;
	vertexArrayCount = 0;
	vertexSizeInBytes = 0;
	indexCount = 0;
	indexArrayCount = 0;
	indexByteTotal = 0;
	meshPrimitive = "triangles";

	skinStructure = nullptr;
}

MeshStructure::~MeshStructure()
{
}

bool MeshStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "lod")
	{
		*type = kDataUnsignedInt32;
		*value = &meshLevel;
		return (true);
	}

	if (identifier == "primitive")
	{
		*type = kDataString;
		*value = &meshPrimitive;
		return (true);
	}

	return (false);
}

bool MeshStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kStructureVertexArray) || (type == kStructureIndexArray) || (type == kStructureSkin))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult MeshStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	Structure *structure = GetFirstSubnode();
	while (structure)
	{
		StructureType type = structure->GetStructureType();
		if (type == kStructureVertexArray)
		{
			const VertexArrayStructure *vertexArrayStructure = static_cast<const VertexArrayStructure *>(structure);

			// Process vertex array here.
			//edit
			ScanInfo->AddString((const char*)vertexArrayStructure->GetArrayAttrib());

			if (vertexCount == 0)
				vertexCount = vertexArrayStructure->vertexCount;
			else if (vertexArrayStructure->vertexCount != vertexCount)
				return (kDataOpenGexVertexCountMismatch);

			if (vertexArrayStructure->GetArrayAttrib() == "position")
				flags |= FLAG_HAS_POSITION;
			else if (vertexArrayStructure->GetArrayAttrib() == "texcoord")
				flags |= FLAG_HAS_TEXCOORD;
			else if (vertexArrayStructure->GetArrayAttrib() == "tangent")
				flags |= FLAG_HAS_TANGENT;
			else if (vertexArrayStructure->GetArrayAttrib() == "normal")
				flags |= FLAG_HAS_NORMAL;

			vertexArrayCount++;
			
		}
		else if (type == kStructureIndexArray)
		{
			IndexArrayStructure *indexArrayStructure = static_cast<IndexArrayStructure *>(structure);

			// Process index array here.
			//edit
			indexArrayCount++;
		}
		else if (type == kStructureSkin)
		{
			if (skinStructure)
			{
				return (kDataExtraneousSubstructure);
			}

			skinStructure = static_cast<SkinStructure *>(structure);
		}

		structure = structure->Next();
	}

	// Do application-specific mesh processing here.
	//edit
	meshIndex = ScanInfo->meshCount++;
	uint32_t canCreateTangents = (flags & (FLAG_HAS_POSITION | FLAG_HAS_TEXCOORD | FLAG_HAS_TANGENT | FLAG_HAS_NORMAL)) == (FLAG_HAS_POSITION | FLAG_HAS_TEXCOORD | FLAG_HAS_NORMAL);
	if (canCreateTangents)
	{
		ScanInfo->AddString("tangent");
		ScanInfo->AddString("bitangent");
		ScanInfo->vertexArrayCount += 2;
		ScanInfo->totalVertexByteCount += 6 * vertexCount * sizeof(float);
		vertexArrayCount += 2;
	}

	return (kDataOkay);
}


ObjectStructure::ObjectStructure(StructureType type) : OpenGexStructure(type)
{
	SetBaseStructureType(kStructureObject);
}

ObjectStructure::~ObjectStructure()
{
}


GeometryObjectStructure::GeometryObjectStructure() : ObjectStructure(kStructureGeometryObject)
{
	visibleFlag = true;
	shadowFlag = true;
	motionBlurFlag = true;
}

GeometryObjectStructure::~GeometryObjectStructure()
{
	meshMap.RemoveAll();
}

bool GeometryObjectStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "visible")
	{
		*type = kDataBool;
		*value = &visibleFlag;
		return (true);
	}

	if (identifier == "shadow")
	{
		*type = kDataBool;
		*value = &shadowFlag;
		return (true);
	}

	if (identifier == "motion_blur")
	{
		*type = kDataBool;
		*value = &motionBlurFlag;
		return (true);
	}

	return (false);
}

bool GeometryObjectStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kStructureMesh) || (type == kStructureMorph))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult GeometryObjectStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	int32 meshCount = 0;
	int32 skinCount = 0;

	//edit
	uint32_t minMeshIdx = 0xFFFFFFFF;
	uint32_t maxMeshIdx = 0;

	Structure *structure = GetFirstCoreSubnode();
	while (structure)
	{
		StructureType type = structure->GetStructureType();
		if (type == kStructureMesh)
		{
			MeshStructure *meshStructure = static_cast<MeshStructure *>(structure);
			if (!meshMap.Insert(meshStructure))
			{
				return (kDataOpenGexDuplicateLod);
			}

			//edit
			minMeshIdx = (meshStructure->meshIndex < minMeshIdx) ? meshStructure->meshIndex : minMeshIdx;
			maxMeshIdx = (meshStructure->meshIndex > maxMeshIdx) ? meshStructure->meshIndex : maxMeshIdx;

			meshCount++;
			skinCount += (meshStructure->GetSkinStructure() != nullptr);
		}
		else if (type == kStructureMorph)
		{
			MorphStructure *morphStructure = static_cast<MorphStructure *>(structure);
			if (!morphMap.Insert(morphStructure))
			{
				return (kDataOpenGexDuplicateMorph);
			}
		}

		structure = structure->Next();
	}

	if (meshCount == 0)
	{
		return (kDataMissingSubstructure);
	}

	if ((skinCount != 0) && (skinCount != meshCount))
	{
		return (kDataOpenGexMissingLodSkin);
	}

	// Do application-specific object processing here.
	//edit
	assert(minMeshIdx != 0xFFFFFFFF);
	assert((maxMeshIdx + 1) - minMeshIdx == meshCount);
	modelIndex = ScanInfo->modelCount++;
	this->meshStart = minMeshIdx;
	this->meshCount = meshCount;

	return (kDataOkay);
}


LightObjectStructure::LightObjectStructure() : ObjectStructure(kStructureLightObject)
{
	shadowFlag = true;
}

LightObjectStructure::~LightObjectStructure()
{
}

bool LightObjectStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "type")
	{
		*type = kDataString;
		*value = &typeString;
		return (true);
	}

	if (identifier == "shadow")
	{
		*type = kDataBool;
		*value = &shadowFlag;
		return (true);
	}

	return (false);
}

bool LightObjectStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if ((structure->GetBaseStructureType() == kStructureAttrib) || (structure->GetStructureType() == kStructureAtten))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult LightObjectStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	if (typeString == "infinite")
	{
		// Prepare to handle infinite light here.
	}
	else if (typeString == "point")
	{
		// Prepare to handle point light here.
	}
	else if (typeString == "spot")
	{
		// Prepare to handle spot light here.
	}
	else
	{
		return (kDataOpenGexUndefinedLightType);
	}

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		StructureType type = structure->GetStructureType();
		if (type == kStructureColor)
		{
			const ColorStructure *colorStructure = static_cast<const ColorStructure *>(structure);
			if (colorStructure->GetAttribString() == "light")
			{
				// Process light color here.
			}
		}
		else if (type == kStructureParam)
		{
			const ParamStructure *paramStructure = static_cast<const ParamStructure *>(structure);
			if (paramStructure->GetAttribString() == "intensity")
			{
				// Process light intensity here.
			}
		}
		else if (type == kStructureTexture)
		{
			const TextureStructure *textureStructure = static_cast<const TextureStructure *>(structure);
			if (textureStructure->GetAttribString() == "projection")
			{
				const char *textureName = textureStructure->GetTextureName();

				// Process light texture here.
			}
		}
		else if (type == kStructureAtten)
		{
			const AttenStructure *attenStructure = static_cast<const AttenStructure *>(structure);
			const String& attenKind = attenStructure->GetAttenKind();
			const String& curveType = attenStructure->GetCurveType();

			if (attenKind == "distance")
			{
				if ((curveType == "linear") || (curveType == "smooth"))
				{
					float beginParam = attenStructure->GetBeginParam();
					float endParam = attenStructure->GetEndParam();

					// Process linear or smooth attenuation here.
				}
				else if (curveType == "inverse")
				{
					float scaleParam = attenStructure->GetScaleParam();
					float linearParam = attenStructure->GetLinearParam();

					// Process inverse attenuation here.
				}
				else if (curveType == "inverse_square")
				{
					float scaleParam = attenStructure->GetScaleParam();
					float quadraticParam = attenStructure->GetQuadraticParam();

					// Process inverse square attenuation here.
				}
				else
				{
					return (kDataOpenGexUndefinedCurve);
				}
			}
			else if (attenKind == "angle")
			{
				float endParam = attenStructure->GetEndParam();

				// Process angular attenutation here.
			}
			else if (attenKind == "cos_angle")
			{
				float endParam = attenStructure->GetEndParam();

				// Process angular attenutation here.
			}
			else
			{
				return (kDataOpenGexUndefinedAtten);
			}
		}

		structure = structure->Next();
	}

	// Do application-specific object processing here.

	return (kDataOkay);
}


CameraObjectStructure::CameraObjectStructure() : ObjectStructure(kStructureCameraObject)
{
}

CameraObjectStructure::~CameraObjectStructure()
{
}

bool CameraObjectStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureParam)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult CameraObjectStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	focalLength = 2.0F;
	nearDepth = 0.1F;
	farDepth = 1000.0F;

	const OpenGexDataDescription *openGexDataDescription = static_cast<OpenGexDataDescription *>(dataDescription);
	float distanceScale = openGexDataDescription->GetDistanceScale();
	float angleScale = openGexDataDescription->GetAngleScale();

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kStructureParam)
		{
			const ParamStructure *paramStructure = static_cast<const ParamStructure *>(structure);
			const String& attribString = paramStructure->GetAttribString();
			float param = paramStructure->GetParam();

			if (attribString == "fov")
			{
				float t = tanf(param * angleScale * 0.5F);
				if (t > 0.0F)
				{
					focalLength = 1.0F / t;
				}
			}
			else if (attribString == "near")
			{
				if (param > 0.0F)
				{
					nearDepth = param * distanceScale;
				}
			}
			else if (attribString == "far")
			{
				if (param > 0.0F)
				{
					farDepth = param * distanceScale;
				}
			}

			//edit
			ScanInfo->AddString((const char*)attribString);
		}

		structure = structure->Next();
	}

	// Do application-specific object processing here.

	return (kDataOkay);
}


AttribStructure::AttribStructure(StructureType type) : OpenGexStructure(type)
{
	SetBaseStructureType(kStructureAttrib);
}

AttribStructure::~AttribStructure()
{
}

bool AttribStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "attrib")
	{
		*type = kDataString;
		*value = &attribString;
		return (true);
	}

	return (false);
}


ParamStructure::ParamStructure() : AttribStructure(kStructureParam)
{
}

ParamStructure::~ParamStructure()
{
}

bool ParamStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
		unsigned_int32 arraySize = dataStructure->GetArraySize();
		return (arraySize == 0);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult ParamStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	if (dataStructure->GetDataElementCount() == 1)
	{
		param = dataStructure->GetDataElement(0);
	}
	else
	{
		return (kDataInvalidDataFormat);
	}

	return (kDataOkay);
}


ColorStructure::ColorStructure() : AttribStructure(kStructureColor)
{
}

ColorStructure::~ColorStructure()
{
}

bool ColorStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
		unsigned_int32 arraySize = dataStructure->GetArraySize();
		return ((arraySize >= 3) && (arraySize <= 4));
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult ColorStructure::ProcessData(DataDescription *dataDescription)
{
	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	unsigned_int32 arraySize = dataStructure->GetArraySize();
	if (dataStructure->GetDataElementCount() == arraySize)
	{
		const float *data = &dataStructure->GetDataElement(0);

		color[0] = data[0];
		color[1] = data[1];
		color[2] = data[2];

		if (arraySize == 3)
		{
			color[3] = 1.0F;
		}
		else
		{
			color[3] = data[3];
		}
	}
	else
	{
		return (kDataInvalidDataFormat);
	}

	return (kDataOkay);
}


TextureStructure::TextureStructure() : AttribStructure(kStructureTexture)
{
	texcoordIndex = 0;
}

TextureStructure::~TextureStructure()
{
}

bool TextureStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "texcoord")
	{
		*type = kDataUnsignedInt32;
		*value = &texcoordIndex;
		return (true);
	}

	return (AttribStructure::ValidateProperty(dataDescription, identifier, type, value));
}

bool TextureStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kDataString) || (type == kStructureAnimation) || (structure->GetBaseStructureType() == kStructureMatrix))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult TextureStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = AttribStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	//edit
	ScanInfo->AddString((const char*)GetAttribString());

	bool nameFlag = false;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kDataString)
		{
			if (!nameFlag)
			{
				nameFlag = true;

				const DataStructure<StringDataType> *dataStructure = static_cast<const DataStructure<StringDataType> *>(structure);
				if (dataStructure->GetDataElementCount() == 1)
				{
					textureName = dataStructure->GetDataElement(0);
					ScanInfo->AddString(textureName);
				}
				else
				{
					return (kDataInvalidDataFormat);
				}
			}
			else
			{
				return (kDataExtraneousSubstructure);
			}
		}
		else if (structure->GetBaseStructureType() == kStructureMatrix)
		{
			const MatrixStructure *matrixStructure = static_cast<const MatrixStructure *>(structure);

			// Process transform matrix here.
		}

		structure = structure->Next();
	}

	if (!nameFlag)
	{
		return (kDataMissingSubstructure);
	}

	return (kDataOkay);
}


AttenStructure::AttenStructure() :
		OpenGexStructure(kStructureAtten),
		attenKind("distance"),
		curveType("linear")
{
	beginParam = 0.0F;
	endParam = 1.0F;

	scaleParam = 1.0F;
	offsetParam = 0.0F;

	constantParam = 0.0F;
	linearParam = 0.0F;
	quadraticParam = 1.0F;

	powerParam = 1.0F;
}

AttenStructure::~AttenStructure()
{
}

bool AttenStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "kind")
	{
		*type = kDataString;
		*value = &attenKind;
		return (true);
	}

	if (identifier == "curve")
	{
		*type = kDataString;
		*value = &curveType;
		return (true);
	}

	return (false);
}

bool AttenStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureParam)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult AttenStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	if (curveType == "inverse")
	{
		linearParam = 1.0F;
	}

	const OpenGexDataDescription *openGexDataDescription = static_cast<OpenGexDataDescription *>(dataDescription);
	float distanceScale = openGexDataDescription->GetDistanceScale();
	float angleScale = openGexDataDescription->GetAngleScale();

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kStructureParam)
		{
			const ParamStructure *paramStructure = static_cast<const ParamStructure *>(structure);
			const String& attribString = paramStructure->GetAttribString();

			if (attribString == "begin")
			{
				beginParam = paramStructure->GetParam();

				if (attenKind == "distance")
				{
					beginParam *= distanceScale;
				}
				else if (attenKind == "angle")
				{
					beginParam *= angleScale;
				}
			}
			else if (attribString == "end")
			{
				endParam = paramStructure->GetParam();

				if (attenKind == "distance")
				{
					endParam *= distanceScale;
				}
				else if (attenKind == "angle")
				{
					endParam *= angleScale;
				}
			}
			else if (attribString == "scale")
			{
				scaleParam = paramStructure->GetParam();

				if (attenKind == "distance")
				{
					scaleParam *= distanceScale;
				}
				else if (attenKind == "angle")
				{
					scaleParam *= angleScale;
				}
			}
			else if (attribString == "offset")
			{
				offsetParam = paramStructure->GetParam();
			}
			else if (attribString == "constant")
			{
				constantParam = paramStructure->GetParam();
			}
			else if (attribString == "linear")
			{
				linearParam = paramStructure->GetParam();
			}
			else if (attribString == "quadratic")
			{
				quadraticParam = paramStructure->GetParam();
			}
			else if (attribString == "power")
			{
				powerParam = paramStructure->GetParam();
			}
		}

		structure = structure->Next();
	}

	return (kDataOkay);
}


MaterialStructure::MaterialStructure() : OpenGexStructure(kStructureMaterial)
{
	twoSidedFlag = false;
	materialName = nullptr;

	//edit
	textureCount = 0;
}

MaterialStructure::~MaterialStructure()
{
}

bool MaterialStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "two_sided")
	{
		*type = kDataBool;
		*value = &twoSidedFlag;
		return (true);
	}

	return (false);
}

bool MaterialStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if ((structure->GetBaseStructureType() == kStructureAttrib) || (structure->GetStructureType() == kStructureName))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult MaterialStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const Structure *structure = GetFirstSubstructure(kStructureName);
	if (structure)
	{
		if (GetLastSubstructure(kStructureName) != structure)
		{
			return (kDataExtraneousSubstructure);
		}

		//edit
		materialName = (const char*)static_cast<const OGEX::NameStructure*>(structure)->GetName();
		ScanInfo->AddString(materialName);
	}

	// Do application-specific material processing here.
	//edit
	// todo: fix this properly
	structure = GetFirstSubstructure(kStructureTexture);
	while (structure)
	{
		if (structure->GetStructureType() == kStructureTexture)
		{
			textureCount++;
			ScanInfo->textureReferenceCount++;
		}
		structure = structure->Next();
	}
	materialIndex = ScanInfo->materialCount++;

	return (kDataOkay);
}


KeyStructure::KeyStructure() :
		OpenGexStructure(kStructureKey),
		keyKind("value")
{
}

KeyStructure::~KeyStructure()
{
}

bool KeyStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "kind")
	{
		*type = kDataString;
		*value = &keyKind;
		return (true);
	}

	return (false);
}

bool KeyStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kDataFloat)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult KeyStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const Structure *structure = GetFirstCoreSubnode();
	if (!structure)
	{
		return (kDataMissingSubstructure);
	}

	if (GetLastCoreSubnode() != structure)
	{
		return (kDataExtraneousSubstructure);
	}

	const DataStructure<FloatDataType> *dataStructure = static_cast<const DataStructure<FloatDataType> *>(structure);
	if (dataStructure->GetDataElementCount() == 0)
	{
		return (kDataOpenGexEmptyKeyStructure);
	}

	if ((keyKind == "value") || (keyKind == "-control") || (keyKind == "+control"))
	{
		scalarFlag = false;
	}
	else if ((keyKind == "tension") || (keyKind == "continuity") || (keyKind == "bias"))
	{
		scalarFlag = true;

		if (dataStructure->GetArraySize() != 0)
		{
			return (kDataInvalidDataFormat);
		}
	}
	else
	{
		return (kDataOpenGexInvalidKeyKind);
	}

	return (kDataOkay);
}


CurveStructure::CurveStructure(StructureType type) :
		OpenGexStructure(type),
		curveType("linear")
{
	SetBaseStructureType(kStructureCurve);
}

CurveStructure::~CurveStructure()
{
}

bool CurveStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "curve")
	{
		*type = kDataString;
		*value = &curveType;
		return (true);
	}

	return (false);
}

bool CurveStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureKey)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult CurveStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	keyValueStructure = nullptr;
	keyControlStructure[0] = nullptr;
	keyControlStructure[1] = nullptr;
	keyTensionStructure = nullptr;
	keyContinuityStructure = nullptr;
	keyBiasStructure = nullptr;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kStructureKey)
		{
			const KeyStructure *keyStructure = static_cast<const KeyStructure *>(structure);
			const String& keyKind = keyStructure->GetKeyKind();

			if (keyKind == "value")
			{
				if (!keyValueStructure)
				{
					keyValueStructure = keyStructure;
				}
				else
				{
					return (kDataExtraneousSubstructure);
				}
			}
			else if (keyKind == "-control")
			{
				if (curveType != "bezier")
				{
					return (kDataOpenGexInvalidKeyKind);
				}

				if (!keyControlStructure[0])
				{
					keyControlStructure[0] = keyStructure;
				}
				else
				{
					return (kDataExtraneousSubstructure);
				}
			}
			else if (keyKind == "+control")
			{
				if (curveType != "bezier")
				{
					return (kDataOpenGexInvalidKeyKind);
				}

				if (!keyControlStructure[1])
				{
					keyControlStructure[1] = keyStructure;
				}
				else
				{
					return (kDataExtraneousSubstructure);
				}
			}
			else if (keyKind == "tension")
			{
				if (curveType != "tcb")
				{
					return (kDataOpenGexInvalidKeyKind);
				}

				if (!keyTensionStructure)
				{
					keyTensionStructure = keyStructure;
				}
				else
				{
					return (kDataExtraneousSubstructure);
				}
			}
			else if (keyKind == "continuity")
			{
				if (curveType != "tcb")
				{
					return (kDataOpenGexInvalidKeyKind);
				}

				if (!keyContinuityStructure)
				{
					keyContinuityStructure = keyStructure;
				}
				else
				{
					return (kDataExtraneousSubstructure);
				}
			}
			else if (keyKind == "bias")
			{
				if (curveType != "tcb")
				{
					return (kDataOpenGexInvalidKeyKind);
				}

				if (!keyBiasStructure)
				{
					keyBiasStructure = keyStructure;
				}
				else
				{
					return (kDataExtraneousSubstructure);
				}
			}
		}

		structure = structure->Next();
	}

	if (!keyValueStructure)
	{
		return (kDataMissingSubstructure);
	}

	if (curveType == "bezier")
	{
		if ((!keyControlStructure[0]) || (!keyControlStructure[1]))
		{
			return (kDataMissingSubstructure);
		}
	}
	else if (curveType == "tcb")
	{
		if ((!keyTensionStructure) || (!keyContinuityStructure) || (!keyBiasStructure))
		{
			return (kDataMissingSubstructure);
		}
	}

	return (kDataOkay);
}


TimeStructure::TimeStructure() : CurveStructure(kStructureTime)
{
}

TimeStructure::~TimeStructure()
{
}

DataResult TimeStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = CurveStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const String& curveType = GetCurveType();
	if ((curveType != "linear") && (curveType != "bezier"))
	{
		return (kDataOpenGexInvalidCurveType);
	}

	int32 elementCount = 0;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		if (structure->GetStructureType() == kStructureKey)
		{
			const KeyStructure *keyStructure = static_cast<const KeyStructure *>(structure);
			const DataStructure<FloatDataType> *dataStructure = static_cast<DataStructure<FloatDataType> *>(keyStructure->GetFirstCoreSubnode());
			if (dataStructure->GetArraySize() != 0)
			{
				return (kDataInvalidDataFormat);
			}

			int32 count = dataStructure->GetDataElementCount();
			if (elementCount == 0)
			{
				elementCount = count;
			}
			else if (count != elementCount)
			{
				return (kDataOpenGexKeyCountMismatch);
			}
		}

		structure = structure->Next();
	}

	keyDataElementCount = elementCount;
	return (kDataOkay);
}


ValueStructure::ValueStructure() : CurveStructure(kStructureValue)
{
}

ValueStructure::~ValueStructure()
{
}

DataResult ValueStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = CurveStructure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	const String& curveType = GetCurveType();
	if ((curveType != "linear") && (curveType != "bezier") && (curveType != "tcb"))
	{
		return (kDataOpenGexInvalidCurveType);
	}

	const AnimatableStructure *targetStructure = static_cast<TrackStructure *>(GetSuperNode())->GetTargetStructure();
	const Structure *targetDataStructure = targetStructure->GetFirstCoreSubnode();
	if ((targetDataStructure) && (targetDataStructure->GetStructureType() == kDataFloat))
	{
		unsigned_int32 targetArraySize = static_cast<const PrimitiveStructure *>(targetDataStructure)->GetArraySize();
		int32 elementCount = 0;

		const Structure *structure = GetFirstSubnode();
		while (structure)
		{
			if (structure->GetStructureType() == kStructureKey)
			{
				const KeyStructure *keyStructure = static_cast<const KeyStructure *>(structure);
				const DataStructure<FloatDataType> *dataStructure = static_cast<DataStructure<FloatDataType> *>(keyStructure->GetFirstCoreSubnode());
				unsigned_int32 arraySize = dataStructure->GetArraySize();

				if ((!keyStructure->GetScalarFlag()) && (arraySize != targetArraySize))
				{
					return (kDataInvalidDataFormat);
				}

				int32 count = dataStructure->GetDataElementCount() / Max(arraySize, 1);
				if (elementCount == 0)
				{
					elementCount = count;
				}
				else if (count != elementCount)
				{
					return (kDataOpenGexKeyCountMismatch);
				}
			}

			structure = structure->Next();
		}

		keyDataElementCount = elementCount;
	}

	return (kDataOkay);
}


TrackStructure::TrackStructure() : OpenGexStructure(kStructureTrack)
{
	targetStructure = nullptr;
}

TrackStructure::~TrackStructure()
{
}

bool TrackStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "target")
	{
		*type = kDataRef;
		*value = &targetRef;
		return (true);
	}

	return (false);
}

bool TrackStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetBaseStructureType() == kStructureCurve)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult TrackStructure::ProcessData(DataDescription *dataDescription)
{
	if (targetRef.GetGlobalRefFlag())
	{
		return (kDataOpenGexTargetRefNotLocal);
	}

	Structure *target = GetSuperNode()->GetSuperNode()->FindStructure(targetRef);
	if (!target)
	{
		return (kDataBrokenRef);
	}

	if ((target->GetBaseStructureType() != kStructureMatrix) && (target->GetStructureType() != kStructureMorphWeight))
	{
		return (kDataOpenGexInvalidTargetStruct);
	}

	targetStructure = static_cast<AnimatableStructure *>(target);

	timeStructure = nullptr;
	valueStructure = nullptr;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		StructureType type = structure->GetStructureType();
		if (type == kStructureTime)
		{
			if (!timeStructure)
			{
				timeStructure = static_cast<const TimeStructure *>(structure);
			}
			else
			{
				return (kDataExtraneousSubstructure);
			}
		}
		else if (type == kStructureValue)
		{
			if (!valueStructure)
			{
				valueStructure = static_cast<const ValueStructure *>(structure);
			}
			else
			{
				return (kDataExtraneousSubstructure);
			}
		}

		structure = structure->Next();
	}

	if ((!timeStructure) || (!valueStructure))
	{
		return (kDataMissingSubstructure);
	}

	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	if (timeStructure->GetKeyDataElementCount() != valueStructure->GetKeyDataElementCount())
	{
		return (kDataOpenGexKeyCountMismatch);
	}

	// Do application-specific track processing here.

	return (kDataOkay);
}


AnimationStructure::AnimationStructure() : OpenGexStructure(kStructureAnimation)
{
	clipIndex = 0;
	beginFlag = false;
	endFlag = false;
}

AnimationStructure::~AnimationStructure()
{
}

bool AnimationStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "clip")
	{
		*type = kDataInt32;
		*value = &clipIndex;
		return (true);
	}

	if (identifier == "begin")
	{
		beginFlag = true;
		*type = kDataFloat;
		*value = &beginTime;
		return (true);
	}

	if (identifier == "end")
	{
		endFlag = true;
		*type = kDataFloat;
		*value = &endTime;
		return (true);
	}

	return (false);
}

bool AnimationStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	if (structure->GetStructureType() == kStructureTrack)
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult AnimationStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	if (!GetFirstSubstructure(kStructureTrack))
	{
		return (kDataMissingSubstructure);
	}

	// Do application-specific animation processing here.

	return (kDataOkay);
}


ClipStructure::ClipStructure() : OpenGexStructure(kStructureClip)
{
	clipIndex = 0;
}

ClipStructure::~ClipStructure()
{
}

bool ClipStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "index")
	{
		*type = kDataUnsignedInt32;
		*value = &clipIndex;
		return (true);
	}

	return (false);
}

bool ClipStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	StructureType type = structure->GetStructureType();
	if ((type == kStructureName) || (type == kStructureParam))
	{
		return (true);
	}

	return (OpenGexStructure::ValidateSubstructure(dataDescription, structure));
}

DataResult ClipStructure::ProcessData(DataDescription *dataDescription)
{
	DataResult result = Structure::ProcessData(dataDescription);
	if (result != kDataOkay)
	{
		return (result);
	}

	frameRate = 0.0F;
	clipName = nullptr;

	const Structure *structure = GetFirstSubnode();
	while (structure)
	{
		StructureType type = structure->GetStructureType();
		if (type == kStructureName)
		{
			if (clipName)
			{
				return (kDataExtraneousSubstructure);
			}

			clipName = static_cast<const NameStructure *>(structure)->GetName();
		}
		else if (type == kStructureParam)
		{
			const ParamStructure *paramStructure = static_cast<const ParamStructure *>(structure);
			if (paramStructure->GetAttribString() == "rate")
			{
				frameRate = paramStructure->GetParam();
			}
		}

		structure = structure->Next();
	}

	return (kDataOkay);
}


ExtensionStructure::ExtensionStructure() : OpenGexStructure(kStructureExtension)
{
}

ExtensionStructure::~ExtensionStructure()
{
}

bool ExtensionStructure::ValidateProperty(const DataDescription *dataDescription, const String& identifier, DataType *type, void **value)
{
	if (identifier == "applic")
	{
		*type = kDataString;
		*value = &applicationString;
		return (true);
	}

	if (identifier == "type")
	{
		*type = kDataString;
		*value = &typeString;
		return (true);
	}

	return (false);
}

bool ExtensionStructure::ValidateSubstructure(const DataDescription *dataDescription, const Structure *structure) const
{
	return ((structure->GetBaseStructureType() == kStructurePrimitive) || (structure->GetStructureType() == kStructureExtension));
}


OpenGexDataDescription::OpenGexDataDescription()
{
	distanceScale = 1.0F;
	angleScale = 1.0F;
	timeScale = 1.0F;
	upDirection = 2;
}

OpenGexDataDescription::~OpenGexDataDescription()
{
}

Structure *OpenGexDataDescription::CreateStructure(const String& identifier) const
{
	if (identifier == "Metric")
	{
		return (new MetricStructure);
	}

	if (identifier == "Name")
	{
		return (new NameStructure);
	}

	if (identifier == "ObjectRef")
	{
		return (new ObjectRefStructure);
	}

	if (identifier == "MaterialRef")
	{
		return (new MaterialRefStructure);
	}

	if (identifier == "Transform")
	{
		return (new TransformStructure);
	}

	if (identifier == "Translation")
	{
		return (new TranslationStructure);
	}

	if (identifier == "Rotation")
	{
		return (new RotationStructure);
	}

	if (identifier == "Scale")
	{
		return (new ScaleStructure);
	}

	if (identifier == "MorphWeight")
	{
		return (new MorphWeightStructure);
	}

	if (identifier == "Node")
	{
		return (new NodeStructure);
	}

	if (identifier == "BoneNode")
	{
		return (new BoneNodeStructure);
	}

	if (identifier == "GeometryNode")
	{
		return (new GeometryNodeStructure);
	}

	if (identifier == "LightNode")
	{
		return (new LightNodeStructure);
	}

	if (identifier == "CameraNode")
	{
		return (new CameraNodeStructure);
	}

	if (identifier == "VertexArray")
	{
		return (new VertexArrayStructure);
	}

	if (identifier == "IndexArray")
	{
		return (new IndexArrayStructure);
	}

	if (identifier == "BoneRefArray")
	{
		return (new BoneRefArrayStructure);
	}

	if (identifier == "BoneCountArray")
	{
		return (new BoneCountArrayStructure);
	}

	if (identifier == "BoneIndexArray")
	{
		return (new BoneIndexArrayStructure);
	}

	if (identifier == "BoneWeightArray")
	{
		return (new BoneWeightArrayStructure);
	}

	if (identifier == "Skeleton")
	{
		return (new SkeletonStructure);
	}

	if (identifier == "Skin")
	{
		return (new SkinStructure);
	}

	if (identifier == "Morph")
	{
		return (new MorphStructure);
	}

	if (identifier == "Mesh")
	{
		return (new MeshStructure);
	}

	if (identifier == "GeometryObject")
	{
		return (new GeometryObjectStructure);
	}

	if (identifier == "LightObject")
	{
		return (new LightObjectStructure);
	}

	if (identifier == "CameraObject")
	{
		return (new CameraObjectStructure);
	}

	if (identifier == "Param")
	{
		return (new ParamStructure);
	}

	if (identifier == "Color")
	{
		return (new ColorStructure);
	}

	if (identifier == "Texture")
	{
		return (new TextureStructure);
	}

	if (identifier == "Atten")
	{
		return (new AttenStructure);
	}

	if (identifier == "Material")
	{
		return (new MaterialStructure);
	}

	if (identifier == "Key")
	{
		return (new KeyStructure);
	}

	if (identifier == "Time")
	{
		return (new TimeStructure);
	}

	if (identifier == "Value")
	{
		return (new ValueStructure);
	}

	if (identifier == "Track")
	{
		return (new TrackStructure);
	}

	if (identifier == "Animation")
	{
		return (new AnimationStructure);
	}

	if (identifier == "Clip")
	{
		return (new ClipStructure);
	}

	if (identifier == "Extension")
	{
		return (new ExtensionStructure);
	}

	return (nullptr);
}

bool OpenGexDataDescription::ValidateTopLevelStructure(const Structure *structure) const
{
	StructureType type = structure->GetBaseStructureType();
	if ((type == kStructureNode) || (type == kStructureObject))
	{
		return (true);
	}

	type = structure->GetStructureType();
	return ((type == kStructureMetric) || (type == kStructureMaterial) || (type == kStructureClip) || (type == kStructureExtension));
}

DataResult OpenGexDataDescription::ProcessData(void)
{
	DataResult result = DataDescription::ProcessData();
	if (result == kDataOkay)
	{
		Structure *structure = GetRootStructure()->GetFirstSubnode();
		while (structure)
		{
			// Do something with the node here.

			structure = structure->Next();
		}
	}

	return (result);
}

void OgexReadNode(OgexScanInfo* scanInfo, scene_s* sceneInfo, const ODDL::Structure* node, glm::mat4* parentSceneTransform)
{
	glm::mat4 sceneTransform = *parentSceneTransform;

	switch (node->GetStructureType())
	{
	case ODDL::kStructureRoot:
		break;

	case OGEX::kStructureNode:
	case OGEX::kStructureBoneNode:
	case OGEX::kStructureCameraNode:
	case OGEX::kStructureLightNode:
	case OGEX::kStructureGeometryNode:
		sceneTransform = *parentSceneTransform * (static_cast<const OGEX::NodeStructure*> (node)->localTransform);
		break;
	}

	switch (node->GetStructureType())
	{
	case ODDL::kStructureRoot:
		break;

	case OGEX::kStructureNode:
	case OGEX::kStructureBoneNode:
	case OGEX::kStructureCameraNode:
		//assert ( false );
		break;
	case OGEX::kStructureLightNode:
	{
		const OGEX::LightNodeStructure* lightNode = static_cast<const OGEX::LightNodeStructure*> (node);
		const OGEX::LightObjectStructure* lightObject = lightNode->lightObjectStructure;

		String lightType = lightObject->GetTypeString();
		if (lightType == "point")
		{
			/*
			assert(sceneInfo->pointLightCount);
			sceneInfo->pointLightCount--;
			point_light_t* light = sceneInfo->pointLights++;
			light->transform = sceneTransform;
			light->color[0] = lightObject->color[0], light->color[1] = lightObject->color[1], light->color[2] = lightObject->color[2];
			light->constantAttenuation = lightObject->constantAttenuation, light->linearAttenuation = lightObject->linearAttenuation, light->quadraticAttenuation = lightObject->quadraticAttenuation;
			light->attenuationScale = lightObject->attenuationScale, light->attenuationOffset = lightObject->attenuationOffset;

			light->flags = 0;
			if (lightObject->GetShadowFlag())
				light->flags |= SCENE_LIGHT_FLAG_SHADOWS;

				*/
		}
		else if (lightType == "spot")
		{
		/*	assert(sceneInfo->spotLightCount);
			sceneInfo->spotLightCount--;
			spot_light_t* light = sceneInfo->spotLights++;
			light->transform = sceneTransform;
			light->color[0] = lightObject->color[0], light->color[1] = lightObject->color[1], light->color[2] = lightObject->color[2];
			light->constantAttenuation = lightObject->constantAttenuation, light->linearAttenuation = lightObject->linearAttenuation, light->quadraticAttenuation = lightObject->quadraticAttenuation;
			light->attenuationScale = lightObject->attenuationScale, light->attenuationOffset = lightObject->attenuationOffset;
			light->outerAngle = lightObject->outerAngle, light->innerAngle = lightObject->innerAngle;

			light->flags = 0;
			if (lightObject->GetShadowFlag())
				light->flags |= SCENE_LIGHT_FLAG_SHADOWS;
				*/
		}
		else if (lightType == "infinite")
		{
		/*	assert(sceneInfo->directionalLightCount);
			sceneInfo->directionalLightCount--;
			rvm_aos_vec3 forward = { 0.0f, 0.0f, -1.0f };
			rvm_aos_vec3 direction;
			rvm_aos_mat4_mul_aos_vec3w0(&direction, &sceneTransform, &forward);

			directional_light_t* light = sceneInfo->directionalLights++;
			rvm_aos_vec3_normalize((rvm_aos_vec3*)light->direction, &direction);
			light->color[0] = lightObject->color[0], light->color[1] = lightObject->color[1], light->color[2] = lightObject->color[2];

			light->flags = 0;
			if (lightObject->GetShadowFlag())
				light->flags |= SCENE_LIGHT_FLAG_SHADOWS;
				*/
		}
	}
	break;
	case OGEX::kStructureGeometryNode:
	{
		assert(sceneInfo->modelReferenceCount);
		sceneInfo->modelReferenceCount--;
		const OGEX::GeometryNodeStructure* geom = static_cast<const OGEX::GeometryNodeStructure*> (node);
		model_ref_s* modelRef = sceneInfo->modelRefs++;
		modelRef->nameOffset = scanInfo->FindStringOffset(geom->GetNodeName());
		modelRef->modelIndex = geom->GetGeometryObject()->modelIndex;
		modelRef->materialIndices = sceneInfo->materialIndices;
		modelRef->materialIndexCount = geom->GetMaterialCount();
		modelRef->transform = sceneTransform;

		for (uint32_t i = 0; i < geom->GetMaterialCount(); i++)
		{
			modelRef->materialIndices[i] = geom->GetMaterial(i)->materialIndex;
		}

		assert(sceneInfo->materialIndexCount >= geom->GetMaterialCount());
		sceneInfo->materialIndexCount -= geom->GetMaterialCount();
		sceneInfo->materialIndices += geom->GetMaterialCount();
	}
	break;
	default:
		return;
	}

	const ODDL::Structure* subNode = node->GetFirstSubnode();
	while (subNode)
	{
		OgexReadNode(scanInfo, sceneInfo, subNode, &sceneTransform);
		subNode = subNode->Next();
	}
}

void OgexReadRootNode(OgexScanInfo* scanInfo, scene_s* sceneInfo, const ODDL::Structure* rootNode, const OGEX::OpenGexDataDescription* desc)
{
	glm::mat4 transform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	switch (desc->GetUpDirection())
	{
	case 0:	// x = up
		transform = 
		{
			0.0f, 1.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		break;
	case 1:	// y = up -> no transform
		break;
	case 2: // z = up
		transform = 
		{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		break;
	}

	//read the description nodes refereing to geometry objects
	OgexReadNode(scanInfo, sceneInfo, rootNode, &transform);

	Memory_Linear_Allocator* tempAlloc = CreateVirtualMemoryAllocator(ALLOCATOR_IDX_TEMP, 0xFFFFFFFF);
	static int a = 0;
	a++;
	assert(tempAlloc);

	const ODDL::Structure* subNode = rootNode->GetFirstSubnode();
	while (subNode)
	{
		switch (subNode->GetStructureType())
		{
		case OGEX::kStructureExtension:
			assert(false);
			break;

		case OGEX::kStructureNode:
		case OGEX::kStructureBoneNode:
		case OGEX::kStructureCameraNode:
		case OGEX::kStructureLightNode:
		case OGEX::kStructureGeometryNode:
			break;

		case OGEX::kStructureGeometryObject:
			{
				assert(sceneInfo->modelCount);
				sceneInfo->modelCount--;
				const OGEX::GeometryObjectStructure* geom = static_cast<const OGEX::GeometryObjectStructure*> (subNode);
				model_s* model = sceneInfo->models++;
				model->meshStartIndex = geom->meshStart;
				model->meshCount = geom->meshCount;

				Structure *structure = geom->GetFirstCoreSubnode();
				while (structure)
				{
					StructureType type = structure->GetStructureType();
					if (type == OGEX::kStructureMesh)
					{
						const OGEX::MeshStructure* mesh = static_cast<const OGEX::MeshStructure*> (structure);
						assert(mesh->meshIndex >= model->meshStartIndex && mesh->meshIndex < model->meshStartIndex + model->meshCount);

						const ODDL::String& prim = mesh->GetMeshPrimitive();
						if (prim == "triangles")
							sceneInfo->meshes[mesh->meshIndex].primitiveType = MESH_PRIMITIVE_TYPE_TRIANGLE_LIST;
						else
							assert(false);

						sceneInfo->meshes[mesh->meshIndex].vertexBufferStartIndex = sceneInfo->vertexBufferCount;
						sceneInfo->meshes[mesh->meshIndex].indexBufferStartIndex = sceneInfo->indexBufferCount;

						sceneInfo->meshes[mesh->meshIndex].vertexBufferCount = mesh->vertexArrayCount;
						sceneInfo->meshes[mesh->meshIndex].indexBufferCount = mesh->indexArrayCount;

						sceneInfo->meshes[mesh->meshIndex].vertexCount = mesh->vertexCount;

						const OGEX::VertexArrayStructure* posVa = NULL;
						const OGEX::VertexArrayStructure* texVa = NULL;
						const OGEX::VertexArrayStructure* normVa = NULL; 

						glm::vec3* vertex = NULL;
						glm::vec2* texcoord = NULL;
						glm::vec3* normal = NULL;

						uint32_t curVbCount = 0;
						uint32_t curIbCount = 0;

						Structure* subStructure = mesh->GetFirstCoreSubnode();
						while (subStructure)
						{
							StructureType type = subStructure->GetStructureType();
							if (type == OGEX::kStructureVertexArray)
							{
								const OGEX::VertexArrayStructure* va = static_cast<const OGEX::VertexArrayStructure*> (subStructure);
								if (va->GetArrayAttrib() == "position")
								{
									posVa = va;
									vertex = (glm::vec3*)(sceneInfo->vertexData + sceneInfo->vertexDataSizeInBytes);
								}
								
								else if (va->GetArrayAttrib() == "texcoord")
								{
									texVa = va;
									texcoord = (glm::vec2*)(sceneInfo->vertexData + sceneInfo->vertexDataSizeInBytes);
								}
								else if (va->GetArrayAttrib() == "normal")
								{
									normVa = va;
									normal = (glm::vec3*)(sceneInfo->vertexData + sceneInfo->vertexDataSizeInBytes);
								}

								vertex_buffer_s* vb = sceneInfo->vertexBuffers + (sceneInfo->meshes[mesh->meshIndex].vertexBufferStartIndex + curVbCount);
								vb->elementType = va->elementType;
								vb->elementCount = va->elementCount;
								vb->vertexCount = va->vertexCount;
								vb->attribStringOffset = scanInfo->FindStringOffset((const char*)va->GetArrayAttrib());
								vb->vertexOffset = sceneInfo->vertexDataSizeInBytes;
								vb->totalSize = va->totalByteSize;

								memcpy(sceneInfo->vertexData + sceneInfo->vertexDataSizeInBytes, va->data, va->totalByteSize);
								sceneInfo->vertexDataSizeInBytes += va->totalByteSize;
								curVbCount++;
							}
							else if (type == OGEX::kStructureIndexArray)
							{
								const OGEX::IndexArrayStructure* ia = static_cast<const OGEX::IndexArrayStructure*> (subStructure);
								index_buffer_s* ib = sceneInfo->indexBuffers + (sceneInfo->meshes[mesh->meshIndex].indexBufferStartIndex + curIbCount);
								ib->indexByteSize = ia->indexSize;
								ib->materialSlotIndex = ia->GetMaterialIndex();
								ib->indexCount = ia->indexCount;
								ib->indexOffset = sceneInfo->indexDataSizeInBytes;
								ib->totalSize = ia->totalByteCount;

								memcpy(sceneInfo->indexData + sceneInfo->indexDataSizeInBytes, ia->data, ib->indexByteSize * ia->indexCount);
								sceneInfo->indexDataSizeInBytes += ia->totalByteCount;
								curIbCount++;
							}

							subStructure = subStructure->Next();
						}

						uint32_t canCreateTangents = (mesh->flags & (FLAG_HAS_POSITION | FLAG_HAS_TEXCOORD | FLAG_HAS_NORMAL | FLAG_HAS_TANGENT)) == (FLAG_HAS_POSITION | FLAG_HAS_TEXCOORD | FLAG_HAS_NORMAL);
						if (canCreateTangents)
						{
							assert(posVa && texVa && normVa);	//check if all vertexarrays are active
							assert(posVa->elementType == VERTEX_BUFFER_ELEMENT_TYPE_FLOAT && posVa->elementCount == 3);
							assert(texVa->elementType == VERTEX_BUFFER_ELEMENT_TYPE_FLOAT && texVa->elementCount == 2);
							assert(normVa->elementType == VERTEX_BUFFER_ELEMENT_TYPE_FLOAT && normVa->elementCount == 3);

							//allocate vertex buffer for the tangent vectors
							vertex_buffer_s* tvb = sceneInfo->vertexBuffers + (sceneInfo->meshes[mesh->meshIndex].vertexBufferStartIndex + curVbCount);
							tvb->elementType = VERTEX_BUFFER_ELEMENT_TYPE_FLOAT;
							tvb->elementCount = 3;
							tvb->vertexCount = posVa->vertexCount;
							tvb->attribStringOffset = scanInfo->FindStringOffset("tangent");
							tvb->vertexOffset = sceneInfo->vertexDataSizeInBytes;
							tvb->totalSize = posVa->vertexCount * 3 * sizeof(float);
							sceneInfo->vertexDataSizeInBytes += mesh->vertexCount * 3 * sizeof(float);

							//allocate vetrex buffer for the bitangent vector
							vertex_buffer_s* bvb = sceneInfo->vertexBuffers + (sceneInfo->meshes[mesh->meshIndex].vertexBufferStartIndex + (curVbCount + 1));
							bvb->elementType = VERTEX_BUFFER_ELEMENT_TYPE_FLOAT;
							bvb->elementCount = 3;
							bvb->vertexCount = posVa->vertexCount;
							bvb->attribStringOffset = scanInfo->FindStringOffset("bitangent");
							bvb->vertexOffset = sceneInfo->vertexDataSizeInBytes;
							bvb->totalSize = posVa->vertexCount * 3 * sizeof(float);
							sceneInfo->vertexDataSizeInBytes += mesh->vertexCount * 3 * sizeof(float);

							glm::vec3* tangent = (glm::vec3*)(sceneInfo->vertexData + tvb->vertexOffset);
							glm::vec3* bitangent = (glm::vec3*)(sceneInfo->vertexData + bvb->vertexOffset);

							uint32_t triangleCount = 0, triIdx = 0;
							for (uint32_t i = 0; i < sceneInfo->meshes[mesh->meshIndex].indexBufferCount; i++)
							{
								index_buffer_s* ib = sceneInfo->indexBuffers + (sceneInfo->meshes[mesh->meshIndex].indexBufferStartIndex + i);
								triangleCount += ib->indexCount / 3;
							}

							glm::vec3* triNorm = (glm::vec3*)AllocateVirtualMemory(ALLOCATOR_IDX_TEMP, tempAlloc, 0x0FFFFFFF);
							glm::vec3* vertTan = (glm::vec3*)AllocateVirtualMemory(ALLOCATOR_IDX_TEMP, tempAlloc, 0x0FFFFFFF);
							glm::vec3* vertBitan = (glm::vec3*)AllocateVirtualMemory(ALLOCATOR_IDX_TEMP, tempAlloc, 0x0FFFFFFF);
							glm::vec3* vertNorm = (glm::vec3*)AllocateVirtualMemory(ALLOCATOR_IDX_TEMP, tempAlloc, 0x0FFFFFFF);
							memset(triNorm, 0, triangleCount * sizeof(glm::vec3));
							memset(vertTan, 0, tvb->vertexCount * sizeof(glm::vec3));
							memset(vertBitan, 0, tvb->vertexCount * sizeof(glm::vec3));
							memset(vertNorm, 0, tvb->vertexCount * sizeof(glm::vec3));


							for (uint32_t i = 0; i < sceneInfo->meshes[mesh->meshIndex].indexBufferCount; i++)
							{
								index_buffer_s* ib = sceneInfo->indexBuffers + (sceneInfo->meshes[mesh->meshIndex].indexBufferStartIndex + i);

								assert(ib->indexByteSize == 4);
								uint32_t* indexStart = (uint32_t*)(sceneInfo->indexData + ib->indexOffset);

								for (uint32_t a = 0; a < ib->indexCount / 3; a++, triIdx++)
								{
									glm::vec3 E21P = { vertex[indexStart[a * 3 + 1]].x - vertex[indexStart[a * 3]].x, vertex[indexStart[a * 3 + 1]].y - vertex[indexStart[a * 3]].y, vertex[indexStart[a * 3 + 1]].z - vertex[indexStart[a * 3]].z };
									glm::vec3 E31P = { vertex[indexStart[a * 3 + 2]].x - vertex[indexStart[a * 3]].x, vertex[indexStart[a * 3 + 2]].y - vertex[indexStart[a * 3]].y, vertex[indexStart[a * 3 + 2]].z - vertex[indexStart[a * 3]].z };

									glm::vec2 E21T = { texcoord[indexStart[a * 3 + 1]].x - texcoord[indexStart[a * 3]].x, texcoord[indexStart[a * 3 + 1]].y - texcoord[indexStart[a * 3]].y };
									glm::vec2 E31T = { texcoord[indexStart[a * 3 + 2]].x - texcoord[indexStart[a * 3]].x, texcoord[indexStart[a * 3 + 2]].y - texcoord[indexStart[a * 3]].y };

									float div = (E21T.x * E31T.y - E31T.x * E21T.y);

									if (isnan(div))
										div = 0.0f;

									glm::vec3 UN, N;
									glm::vec3 vU, vV;
									UN = glm::cross(E21P, E31P);
									N = glm::normalize(UN);

									if (fabsf(div) > 0.001f)
									{
										float areaMul2 = fabsf(div);

										float a = E31T.y / div;
										float b = -E21T.y / div;
										float c = -E31T.x / div;
										float d = E21T.x / div;


										glm::vec3 vUU = { E21P.x * a + E31P.x * b, E21P.y * a + E31P.y * b, E21P.z * a + E31P.z * b };
										glm::vec3 vVU = { E21P.x * c + E31P.x * d, E21P.y * c + E31P.y * d, E21P.z * c + E31P.z * d };
										vU = glm::normalize(vUU);
										vV = glm::normalize(vVU);
										vU.x *= areaMul2, vU.y *= areaMul2, vU.z *= areaMul2;
										vV.x *= areaMul2, vV.y *= areaMul2, vV.z *= areaMul2;
									}
									else
									{
										vU = { 1.0f, 0.0f, 0.0f };
										vV = { 0.0f, 1.0f, 0.0f };
									}

									for (uint32_t j = 0; j < 3; j++)
									{
										uint32_t i1 = indexStart[a * 3 + ((j + 2) % 3)], i2 = indexStart[a * 3 + ((j + 1) % 3)], i0 = indexStart[a * 3 + j];
										glm::vec3* v0 = &vertex[i0];
										glm::vec3* v1 = &vertex[i1];
										glm::vec3* v2 = &vertex[i2];
										glm::vec3  e1 = { v1->x - v0->x, v1->y - v0->y, v1->z - v0->z };
										glm::vec3  e2 = { v2->x - v0->x, v2->y - v0->y, v2->z - v0->z };

										float weight;
										{
											float lq = glm::length(e1) * glm::length(e2);
											if (lq < 0.0001f)
												lq = 0.0001f;

											float f = glm::dot(e1, e2) / lq;

											if (f > 1.0f)
												f = 1.0f;
											else if (f < -1.0f)
												f = -1.0f;

											weight = acosf(f);
										}

										if (weight <= 0.0f)
											weight = 0.0001f;

										{
											vertNorm[i0].x += N.x * weight, vertNorm[i0].y += N.y * weight, vertNorm[i0].z += N.z * weight;
										}

										{
											vertTan[i0].x += vU.x * weight, vertTan[i0].y += vU.y * weight, vertTan[i0].z += vU.z * weight;
											vertBitan[i0].x += vV.x * weight, vertBitan[i0].y += vV.y * weight, vertBitan[i0].z += vV.z * weight;
										}
									}
								}

								for (uint32_t a = 0; a < tvb->vertexCount; a++)
								{

									glm::vec3 T, B, N;
									N = glm::normalize(vertNorm[a]);

									float dotU = glm::dot(vertTan[a], N);
									float dotV = glm::dot(vertBitan[a], N);
									T = { vertTan[a].x - N.x * dotU, vertTan[a].y - N.y * dotU, vertTan[a].z - N.z * dotU };
									B = { vertBitan[a].x - N.x * dotV, vertBitan[a].y - N.y * dotV, vertBitan[a].z - N.z * dotV };

									glm::vec3 TNC;
									TNC = glm::cross(N, T);

									glm::vec3 NT, NB;
									NT = glm::normalize(T);
									NB = glm::normalize(B);

									normal[a] = N;
									tangent[a] = { NT.x, NT.y, NT.z };
									bitangent[a] = { NB.x, NB.y, NB.z };
								}
							}

							ResetVirtualMemory(ALLOCATOR_IDX_TEMP, tempAlloc);
							curVbCount += 2;
						}
						assert ( curVbCount == mesh->vertexArrayCount );

						sceneInfo->vertexBufferCount += mesh->vertexArrayCount;
						sceneInfo->indexBufferCount  += mesh->indexArrayCount;
					}
					structure = structure->Next();
				}
			}
			break;
			case OGEX::kStructureLightObject:
				break;
			case OGEX::kStructureCameraObject:
			case OGEX::kStructureClip:
				assert(false);
				break;
			case OGEX::kStructureMaterial:
			{
				assert(sceneInfo->materialCount);
				sceneInfo->materialCount--;
				const OGEX::MaterialStructure* mat = static_cast<const OGEX::MaterialStructure*> (subNode);
				material_s* material = sceneInfo->materials++;
				material->nameStringOffset = scanInfo->FindStringOffset(mat->GetMaterialName());
				material->textureReferenceStart = sceneInfo->textureReferenceCount;
				material->textureReferenceCount = mat->textureCount;

				sceneInfo->textureReferenceCount += mat->textureCount;

				texture_ref_s* texRef = sceneInfo->textureRefs + material->textureReferenceStart;
				const Structure *structure = mat->GetFirstSubstructure(OGEX::kStructureTexture);
				while (structure)
				{
					if (structure->GetStructureType() == OGEX::kStructureTexture)
					{
						const OGEX::TextureStructure* tex = static_cast<const OGEX::TextureStructure*> (structure);
						texRef->textureIndex = scanInfo->FindTexture(scanInfo->FindStringOffset((const char*)tex->GetTextureName()));
						texRef->attribOffset = scanInfo->FindStringOffset((const char*)tex->GetAttribString());
						texRef++;
					}
					structure = structure->Next();
				}
				//assert ( texRef == material->textureReferences + material->textureReferenceCount );
			}
			break;

			// Ignore
			case OGEX::kStructureMetric:
				break;

			default:
				assert(false);
		}
		subNode = subNode->Next();
	}
	DestroyVirtualMemoryAllocator(ALLOCATOR_IDX_TEMP, tempAlloc);
}


uint32_t ConvertAsset_OpenGEX
(
	asset_s* outAsset,
	const void* data,
	uint64_t dataSizeInBytes,
	const char* basePath,
	uint32_t basePathLength,
	Memory_Linear_Allocator* allocator,
	uint32_t allocatorIdx
)
{
	OgexScanInfo scanInfo = {};
	scanInfo.stringTableSlots = 2048;
	scanInfo.stringTable = (OgexScanInfo::OgexStringTableEntry*)_alloca(scanInfo.stringTableSlots * sizeof(scanInfo.stringTable[0]));
	memset(scanInfo.stringTable, 0, scanInfo.stringTableSlots * sizeof(scanInfo.stringTable[0]));

	ScanInfo = &scanInfo;

	OGEX::OpenGexDataDescription desc;
	ODDL::DataResult res = desc.ProcessText((const char*)data);

	if (res != ODDL::kDataOkay)
		return (uint32_t)res;
	
	uint64_t totalStringLength = 0;
	for (uint32_t i = 0; i < scanInfo.stringTableSlots; i++)
	{
		if (scanInfo.stringTable[i].strPtr)
		{
			totalStringLength += strlen(scanInfo.stringTable[i].strPtr) + 1;
		}
	}

	// Todo: fix this properly
	scanInfo.textureTableSlots = (uint32_t)(scanInfo.textureReferenceCount * 1.5f);
	scanInfo.textureTable = (OgexScanInfo::ogesTextureTableEntry*)_alloca(scanInfo.textureTableSlots * sizeof(scanInfo.textureTable[0]));
	memset(scanInfo.textureTable, 0xFF, scanInfo.textureTableSlots * sizeof(scanInfo.textureTable[0]));

	ODDL::Structure* curStructure = desc.GetRootStructure()->GetFirstSubstructure(OGEX::kStructureMaterial);
	ODDL::Structure* lastStructure = desc.GetRootStructure()->GetLastSubstructure(OGEX::kStructureMaterial);
	while (curStructure)
	{
		if (curStructure->GetStructureType() == OGEX::kStructureMaterial)
		{
			OGEX::MaterialStructure* materialStructure = static_cast<OGEX::MaterialStructure*> (curStructure);
			ODDL::Structure* subStructure = curStructure->GetFirstSubstructure(OGEX::kStructureTexture);
			while (subStructure)
			{
				if (subStructure->GetStructureType() == OGEX::kStructureTexture)
				{
					OGEX::TextureStructure* tex = static_cast<OGEX::TextureStructure*> (subStructure);
					scanInfo.AddTexture(scanInfo.FindStringOffset((const char*)tex->GetTextureName()));
				}
				subStructure = subStructure->Next();
			}
		}

		if (curStructure == lastStructure)
			break;
		else
			curStructure = curStructure->Next();
	}

	uint64_t memorySize =
		sizeof(scene_s)
		+ scanInfo.modelCount * sizeof(model_s)
		+ scanInfo.meshCount * sizeof(mesh_s)
		+ scanInfo.materialCount * sizeof(material_s)
		+ scanInfo.vertexArrayCount * sizeof(vertex_buffer_s)
		+ scanInfo.indexArrayCount * sizeof(index_buffer_s)
		+ scanInfo.modelReferenceCount * sizeof(model_ref_s)
		+ scanInfo.textureCount * sizeof(texture_s)
		+ scanInfo.textureReferenceCount * sizeof(texture_ref_s)
		+ scanInfo.pointLightCount * sizeof(point_light_s)
		+ scanInfo.spotLightCount * sizeof(spot_light_s)
		+ scanInfo.directionalLightCount * sizeof(directional_light_s)
		+ scanInfo.materialReferenceCount * sizeof(uint32_t)
		+ scanInfo.totalVertexByteCount
		+ scanInfo.totalIndexByteCount
		+ totalStringLength;

	uint8_t* memory = (uint8_t*				)AllocateVirtualMemory(ALLOCATOR_IDX_ASSET_DATA, allocator, memorySize);
	scene_s* scene = (scene_s*				)memory;
	scene->models = (model_s*				)(scene + 1);
	scene->meshes = (mesh_s*				)(scene->models + scanInfo.modelCount);
	scene->materials = (material_s*			)(scene->meshes + scanInfo.meshCount);
	scene->vertexBuffers = (vertex_buffer_s*)(scene->materials + scanInfo.materialCount);
	scene->indexBuffers = (index_buffer_s*	)(scene->vertexBuffers + scanInfo.vertexArrayCount);
	scene->modelRefs = (model_ref_s*		)(scene->indexBuffers + scanInfo.indexArrayCount);
	scene->textures = (texture_s*			)(scene->modelRefs + scanInfo.modelReferenceCount);
	scene->textureRefs = (texture_ref_s*	)(scene->textures + scanInfo.textureCount);
	scene->pointLights = (point_light_s*	)(scene->textureRefs + scanInfo.textureReferenceCount);
	scene->spotLights = (spot_light_s*		)(scene->pointLights + scanInfo.pointLightCount);
	scene->directionalLights = (directional_light_s*)(scene->spotLights + scanInfo.spotLightCount);
	scene->materialIndices = (uint32_t*		)(scene->directionalLights + scanInfo.directionalLightCount);
	scene->vertexData = (uint8_t*			)(scene->materialIndices + scanInfo.materialReferenceCount);
	scene->indexData = (uint8_t*			)(scene->vertexData + scanInfo.totalVertexByteCount);
	scene->stringData = (const char*		)(scene->indexData + scanInfo.totalIndexByteCount);

	outAsset->type = 'OGEX';
	outAsset->size = memorySize;
	outAsset->data = memory;

	scene->vertexDataSizeInBytes = scanInfo.totalVertexByteCount;
	scene->indexDataSizeInBytes = scanInfo.totalIndexByteCount;
	scene->stringDataSizeInBytes = totalStringLength;
	scene->modelCount = scanInfo.modelCount;
	scene->vertexBufferCount = scanInfo.vertexArrayCount;
	scene->indexBufferCount = scanInfo.indexArrayCount;
	scene->modelReferenceCount = scanInfo.modelReferenceCount;
	scene->meshCount = scanInfo.meshCount;
	scene->directionalLightCount = scanInfo.directionalLightCount;
	scene->spotLightCount = scanInfo.spotLightCount;
	scene->pointLightCount = scanInfo.pointLightCount;
	scene->materialCount = scanInfo.materialCount;
	scene->textureCount = scanInfo.textureCount;
	scene->textureReferenceCount = scanInfo.textureReferenceCount;
	scene->materialIndexCount = scanInfo.materialReferenceCount;

	totalStringLength = 0;

	for (uint32_t i = 0; i < scanInfo.stringTableSlots; i++)
	{
		if (scanInfo.stringTable[i].strPtr)
		{
			uint64_t len = strlen(scanInfo.stringTable[i].strPtr);
			memcpy((void*)(scene->stringData + totalStringLength), scanInfo.stringTable[i].strPtr, len + 1);
			scanInfo.stringTable[i].offset = totalStringLength;
			totalStringLength += len + 1;
		}
		else
			scanInfo.stringTable[i].offset = 0xFFFFFFFFFFFFFFFFULL;
	}

	scanInfo.textureCount = 0;
	memset(scanInfo.textureTable, 0xFF, scanInfo.textureTableSlots * sizeof(scanInfo.textureTable[0]));
	curStructure = desc.GetRootStructure()->GetFirstSubstructure(OGEX::kStructureMaterial);
	lastStructure = desc.GetRootStructure()->GetLastSubstructure(OGEX::kStructureMaterial);
	while (curStructure)
	{
		if (curStructure->GetStructureType() == OGEX::kStructureMaterial)
		{
			OGEX::MaterialStructure* materialStructure = static_cast<OGEX::MaterialStructure*> (curStructure);
			ODDL::Structure* subStructure = curStructure->GetFirstSubstructure(OGEX::kStructureTexture);
			while (subStructure)
			{
				if (subStructure->GetStructureType() == OGEX::kStructureTexture)
				{
					OGEX::TextureStructure* tex = static_cast<OGEX::TextureStructure*> (subStructure);
					scanInfo.AddTexture(scanInfo.FindStringOffset((const char*)tex->GetTextureName()));
				}
				subStructure = subStructure->Next();
			}
		}

		if (curStructure == lastStructure)
			break;
		else
			curStructure = curStructure->Next();
	}

	for (uint32_t i = 0; i < scanInfo.textureTableSlots; i++)
	{
		if (scanInfo.textureTable[i].textureIndex != 0xFFFFFFFF)
		{
			scene->textures[scanInfo.textureTable[i].textureIndex].pathOffset = scanInfo.textureTable[i].pathOffset;
		}
	}

	scene_s tempScene = *scene;
	tempScene.vertexBufferCount = 0;
	tempScene.indexBufferCount = 0;
	tempScene.vertexDataSizeInBytes = 0;
	tempScene.indexDataSizeInBytes = 0;
	tempScene.textureReferenceCount = 0;
	OgexReadRootNode(&scanInfo, &tempScene, desc.GetRootStructure(), &desc);

	char buffer[512];
	memcpy(buffer, basePath, basePathLength);

	for (uint32_t i = 0; i < scene->textureCount; i++)
	{
		const char* texturePath = scene->stringData + scene->textures[i].pathOffset;
		uint32_t texturePathLength = (uint32_t)strlen(texturePath);
		uint32_t pathLength = basePathLength + texturePathLength;

		assert(pathLength < sizeof(buffer));
		memcpy(buffer + basePathLength, texturePath, texturePathLength);
		buffer[pathLength] = '\0';

		LoadAssetStaticManager(buffer, pathLength);
	}

	return 0;
}