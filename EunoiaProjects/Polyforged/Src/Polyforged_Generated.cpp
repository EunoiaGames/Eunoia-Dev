#include <Eunoia\Eunoia.h>
#include "Polyforged.h"

	template<>
	EU_PROJ metadata_typeid Metadata::GetTypeID < ThirdPersonCameraComponent > () { return Eunoia::Metadata::LastEngineTypeID + 1; }

	template<>
	MetadataInfo Metadata::ConstructMetadataInfo<ThirdPersonCameraComponent>()
	{
		MetadataInfo info;
		info.id = Eunoia::Metadata::LastEngineTypeID + 1;
		info.type = METADATA_CLASS;
		info.cls = Eunoia::Metadata::AllocateClass( false );
		info.cls->name = "ThirdPersonCameraComponent";
		info.cls->baseClassName = "ECSComponent";
		info.cls->baseClassSize = sizeof( ECSComponent );
		info.cls->size = sizeof( ThirdPersonCameraComponent );
		info.cls->isComponent = true;
		info.cls->isSystem = false;
		info.cls->isEvent = false;
		info.cls->DefaultConstructor = Eunoia::MetadataCreateInstance< ThirdPersonCameraComponent >;
		info.cls->members.SetCapacityAndElementCount( 0 );

		return info;
	}

	void Polyforged::RegisterMetadata()
	{
		Metadata::RegisterMetadataInfo( Metadata::ConstructMetadataInfo< ThirdPersonCameraComponent >() );

		Metadata::LastProjectTypeID = Metadata::LastEngineTypeID + 1;
	}