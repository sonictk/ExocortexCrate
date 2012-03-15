#include "Alembic.h"
#include "AlembicDefinitions.h"
#include "AlembicMeshUVWModifier.h"
#include "AlembicArchiveStorage.h"
#include "utility.h"
#include <iparamb2.h>
#include <MeshNormalSpec.h>
#include <Assetmanagement\AssetType.h>
#include "alembic.h"
#include "AlembicXForm.h"
#include "AlembicVisCtrl.h"
#include "ParamBlockUtility.h"


using namespace MaxSDK::AssetManagement;

IObjParam *AlembicMeshUVWModifier::ip              = NULL;
AlembicMeshUVWModifier *AlembicMeshUVWModifier::editMod         = NULL;

static AlembicMeshUVWModifierClassDesc AlembicMeshUVWModifierDesc;
ClassDesc2* GetAlembicMeshUVWModifierClassDesc() {return &AlembicMeshUVWModifierDesc;}

//--- Properties block -------------------------------

static ParamBlockDesc2 AlembicMeshUVWModifierParams(
	0,
	_T(ALEMBIC_MESH_UVW_MODIFIER_SCRIPTNAME),
	0,
	GetAlembicMeshUVWModifierClassDesc(),
	P_AUTO_CONSTRUCT | P_AUTO_UI,
	0,

	// rollout description 
	IDD_ALEMBIC_MESH_UVW_PARAMS, IDS_ALEMBIC, 0, 0, NULL,

    // params
	AlembicMeshUVWModifier::ID_PATH, _T("path"), TYPE_FILENAME, P_RESET_DEFAULT, IDS_PATH,
	    p_default, "",
	    p_ui,        TYPE_EDITBOX,		IDC_PATH_EDIT,
	 	end,
        
	AlembicMeshUVWModifier::ID_IDENTIFIER, _T("identifier"), TYPE_STRING, P_RESET_DEFAULT, IDS_IDENTIFIER,
	    p_default, "",
	    p_ui,        TYPE_EDITBOX,		IDC_IDENTIFIER_EDIT,
	 	end,

	AlembicMeshUVWModifier::ID_TIME, _T("time"), TYPE_FLOAT, P_ANIMATABLE, IDS_TIME,
		p_default,       0.0f,
		p_range,         0.0f, 1000.0f,
		p_ui,            TYPE_SPINNER,       EDITTYPE_FLOAT, IDC_TIME_EDIT,    IDC_TIME_SPIN, 0.5f,
		end,

	AlembicMeshUVWModifier::ID_MUTED, _T("muted"), TYPE_BOOL, P_ANIMATABLE, IDS_MUTED,
		p_default,       TRUE,
		p_ui,            TYPE_SINGLECHEKBOX,  IDC_MUTED_CHECKBOX,
		end,

	end
);

//--- Modifier methods -------------------------------

AlembicMeshUVWModifier::AlembicMeshUVWModifier() 
{
    pblock = NULL;

	GetAlembicMeshUVWModifierClassDesc()->MakeAutoParamBlocks(this);
}

RefTargetHandle AlembicMeshUVWModifier::Clone(RemapDir& remap) 
{
	AlembicMeshUVWModifier *mod = new AlembicMeshUVWModifier();

    mod->ReplaceReference (0, remap.CloneRef(pblock));
	
    BaseClone(this, mod, remap);
	return mod;
}

void AlembicMeshUVWModifier::ModifyObject (TimeValue t, ModContext &mc, ObjectState *os, INode *node) 
{
	ESS_CPP_EXCEPTION_REPORTING_START

	Interval interval = FOREVER;//os->obj->ObjectValidity(t);
	ESS_LOG_INFO( "Interval Start: " << interval.Start() << " End: " << interval.End() );

    MCHAR const* strPath = NULL;
	this->pblock->GetValue( AlembicMeshUVWModifier::ID_PATH, t, strPath, interval);

	MCHAR const* strIdentifier = NULL;
	this->pblock->GetValue( AlembicMeshUVWModifier::ID_IDENTIFIER, t, strIdentifier, interval);
 
	float fTime;
	this->pblock->GetValue( AlembicMeshUVWModifier::ID_TIME, t, fTime, interval);

	BOOL bTopology = false;
	BOOL bGeometry = false;
	float fGeoAlpha = 1.0f;
	BOOL bNormals = false;
	BOOL bUVs = true;

	BOOL bMuted;
	this->pblock->GetValue( AlembicMeshUVWModifier::ID_MUTED, t, bMuted, interval);
	
	ESS_LOG_INFO( "AlembicMeshUVWModifier::ModifyObject strPath: " << strPath << " strIdentifier: " << strIdentifier << " fTime: " << fTime << 
		" bTopology: " << bTopology << " bGeometry: " << bGeometry << " bNormals: " << bNormals << " bUVs: " << bUVs << " bMuted: " << bMuted );

	if( bMuted ) {
		return;
	}

	if( strlen( strPath ) == 0 ) {
	   ESS_LOG_ERROR( "No filename specified." );
	   return;
	}
	if( strlen( strIdentifier ) == 0 ) {
	   ESS_LOG_ERROR( "No path specified." );
	   return;
	}

	if( ! fs::exists( strPath ) ) {
		ESS_LOG_ERROR( "Can't file Alembic file.  Path: " << strPath );
		return;
	}

	Alembic::AbcGeom::IObject iObj;
	try {
		iObj = getObjectFromArchive(strPath, strIdentifier);
	} catch( std::exception exp ) {
		ESS_LOG_ERROR( "Can not open Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier << " reason: " << exp.what() );
		return;
	}

	if(!iObj.valid()) {
		ESS_LOG_ERROR( "Not a valid Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier );
		return;
	}

   alembic_fillmesh_options options;
   options.pIObj = &iObj;
   options.dTicks = GetTimeValueFromSeconds( fTime );
   options.nDataFillFlags = 0;
   options.fVertexAlpha = fGeoAlpha;
    if( bTopology ) {
	   options.nDataFillFlags |= ALEMBIC_DATAFILL_FACELIST;
		options.nDataFillFlags |= ALEMBIC_DATAFILL_FACESETS;
   }
   if( bGeometry ) {
	   options.nDataFillFlags |= ALEMBIC_DATAFILL_VERTEX;
   }
   if( bNormals ) {
	   options.nDataFillFlags |= ALEMBIC_DATAFILL_NORMALS;
   }
   if( bUVs ) {
		options.nDataFillFlags |= ALEMBIC_DATAFILL_UVS;
   }
  
   // Find out if we are modifying a poly object or a tri object
   if (os->obj->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0)))
   {
	   PolyObject *pPolyObj = reinterpret_cast<PolyObject *>(os->obj->ConvertToType(t, Class_ID(POLYOBJ_CLASS_ID, 0)));

	   options.pMNMesh = &( pPolyObj->GetMesh() );
    
	   if (os->obj != pPolyObj) {
          os->obj = pPolyObj;
	   }

   }
   else if (os->obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)))
   {
      TriObject *pTriObj = reinterpret_cast<TriObject *>(os->obj->ConvertToType(t, Class_ID(TRIOBJ_CLASS_ID, 0)));
		options.pMesh = &( pTriObj->GetMesh() );

		if (os->obj != pTriObj) {
          os->obj = pTriObj;
		}
   } 
   else
   {
  		ESS_LOG_ERROR( "Can not convert internal mesh data into a TriObject or PolyObject, confused." );
	     return;
   }

   try {
	   AlembicImport_FillInPolyMesh(options);
   }
   catch(std::exception exp ) {
		ESS_LOG_ERROR( "Error reading mesh from Alembic data stream.  Path: " << strPath << " identifier: " << strIdentifier << " reason: " << exp.what() );
		return;
   }

   // update the validity channel
    if( bTopology ) {
		os->obj->UpdateValidity(TOPO_CHAN_NUM, interval);
		os->obj->UpdateValidity(GEOM_CHAN_NUM, interval);
	}
    if( bGeometry ) {
		os->obj->UpdateValidity(GEOM_CHAN_NUM, interval);
	}
    if( bUVs ) {
		os->obj->UpdateValidity(TEXMAP_CHAN_NUM, interval);
   }

   	ESS_CPP_EXCEPTION_REPORTING_END
}

RefResult AlembicMeshUVWModifier::NotifyRefChanged (Interval changeInt, RefTargetHandle hTarget,
										   PartID& partID, RefMessage message) {
	ESS_CPP_EXCEPTION_REPORTING_START

	switch (message) 
    {
	case REFMSG_CHANGE:
		if (editMod!=this) break;
		
        AlembicMeshUVWModifierParams.InvalidateUI(pblock->LastNotifyParamID());
		break;
 
    case REFMSG_WANT_SHOWPARAMLEVEL:

        break;
	}

	ESS_CPP_EXCEPTION_REPORTING_END

	return REF_SUCCEED;
}


void AlembicMeshUVWModifier::BeginEditParams(IObjParam *ip,ULONG flags,Animatable *prev)
{
	this->ip = ip;
    editMod  = this;

	AlembicMeshUVWModifierDesc.BeginEditParams(ip, this, flags, prev);
    
//    AlembicXFormCtrlDlgProc* dlgProc;
//	dlgProc = new AlembicXFormCtrlDlgProc(this);
//	xform_params_desc.SetUserDlgProc( AlembicXFormCtrl_params, dlgProc );

    // Necessary?
	// NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
}

void AlembicMeshUVWModifier::EndEditParams( IObjParam *ip, ULONG flags, Animatable *next )
{
	AlembicMeshUVWModifierDesc.EndEditParams(ip, this, flags, next);

	this->ip = NULL;
    editMod  = NULL;
}