#ifndef __ALEMBIC_INTERMEDIATE_POLYMESH_3DSMax_H__
#define __ALEMBIC_INTERMEDIATE_POLYMESH_3DSMax_H__


#include "AlembicMax.h"
#include "AlembicIntermediatePolyMesh.h"

class VNormal
{   
public:     
    Point3 norm;     
    DWORD smooth;     
    VNormal *next;     
    BOOL init;      
    VNormal() {smooth=0;next=NULL;init=FALSE;norm=Point3(0,0,0);}     
    VNormal(Point3 &n,DWORD s) {next=NULL;init=TRUE;norm=n;smooth=s;}     
    ~VNormal() {smooth=0;next=NULL;init=FALSE;norm=Point3(0,0,0);}     
    void AddNormal(Point3 &n,DWORD s);     
    Point3 &GetNormal(DWORD s);     
    void Normalize();
};


struct materialStr{
	
	std::string name;
	int matId;

};

typedef std::map<int, materialStr> meshMaterialsMap;
typedef std::map<int, materialStr>::iterator meshMaterialsMap_it;
typedef std::map<int, materialStr>::const_iterator meshMaterialsMap_cit;

typedef std::map<AnimHandle, meshMaterialsMap> mergedMeshMaterialsMap;
typedef std::map<AnimHandle, meshMaterialsMap>::iterator mergedMeshMaterialsMap_it;
typedef std::map<AnimHandle, meshMaterialsMap>::const_iterator mergedMaterialsMap_cit;

struct materialsMergeStr
{
	mergedMeshMaterialsMap groupMatMap;
	AnimHandle currUniqueHandle;
	int nNextMatId;
	bool bPreserveIds;

	materialsMergeStr():currUniqueHandle(NULL), nNextMatId(0), bPreserveIds(false)
	{}

	int getUniqueMatId(int matId);
	materialStr& getMatEntry(AnimHandle uniqueHandle, int matId);

	void setMatName(int matId, const std::string& name);
};

class AlembicWriteJob;


class MeshSmoothingGroupNormals {
private:
	std::vector<VNormal> mNormals;
	Mesh *mpMesh;
	MNMesh *mpPolyMesh;

public:
	MeshSmoothingGroupNormals( Mesh *pMesh ) : mpMesh(pMesh ), mpPolyMesh( NULL ){
		mNormals.resize(pMesh->numVerts);	    
		for (int i = 0; i < pMesh->numFaces; i++) 
		{     
			Face *face = &pMesh->faces[i];
			Point3 faceNormal = pMesh->getFaceNormal(i);
			for (int j=0; j<3; j++) 
			{       
				mNormals[face->v[j]].AddNormal(faceNormal, face->smGroup);     
			}     
		}   
	    
		for (int i=0; i < pMesh->numVerts; i++) 
		{     
			mNormals[i].Normalize(); 
		}
	} 

	MeshSmoothingGroupNormals(MNMesh *pMesh) : mpMesh(NULL ), mpPolyMesh( pMesh ){
		mNormals.resize(pMesh->numv);
		for (int i = 0; i < pMesh->numf; i++) 
		{     
			MNFace *face = &pMesh->f[i];
			Point3 faceNormal = pMesh->GetFaceNormal(i);
			for (int j=0; j<face->deg; j++) 
			{       
				mNormals[face->vtx[j]].AddNormal(faceNormal, face->smGroup);     
			}     
		}   
	    
		for (int i=0; i < pMesh->numv; i++) 
		{     
			mNormals[i].Normalize();   
		}
	}

	~MeshSmoothingGroupNormals() {
		for (int i=0; i < mNormals.size(); i++) 
		{   
			VNormal *ptr = mNormals[i].next;
			while (ptr)
			{
				VNormal *tmp = ptr;
				ptr = ptr->next;
				delete tmp;
			}
		}
		mNormals.clear();  
	}
	
	Point3 GetVNormal(int faceNo, int faceVertNo )
	{
		if( mpMesh != NULL ) {
			// If we do not a smoothing group, we can't base ourselves on anything else,
			// so we can just return the face normal.
			Face *face = &mpMesh->faces[faceNo];
			if (face == NULL || face->smGroup == 0)
			{
				return mpMesh->getFaceNormal(faceNo);
			}

			// Check to see if there is a smoothing group normal
			int vertIndex = face->v[faceVertNo];
			Point3 normal = mNormals[vertIndex].GetNormal(face->smGroup);

			if (normal.LengthSquared() > 0.0f)
			{
				return normal.Normalize();
			}

			// If we did not find any normals or the normals offset each other for some
			// reason, let's just let max tell us what it thinks the normal should be.
			return mpMesh->getNormal(vertIndex);
		}
		else {
			// If we do not a smoothing group, we can't base ourselves on anything else,
			// so we can just return the face normal.
			MNFace *face = mpPolyMesh->F(faceNo);
			if (face == NULL || face->smGroup == 0)
			{
				return mpPolyMesh->GetFaceNormal(faceNo);
			}

			// Check to see if there is a smoothing group normal
			int vertIndex = face->vtx[faceVertNo];
			Point3 normal = mNormals[vertIndex].GetNormal(face->smGroup);

			if (normal.LengthSquared() > 0.0f)
			{
				return normal.Normalize();
			}

			// If we did not find any normals or the normals offset each other for some
			// reason, let's just let max tell us what it thinks the normal should be.
			return mpPolyMesh->GetVertexNormal(vertIndex);
		}
	}
};



class IntermediatePolyMesh3DSMax : public AlembicIntermediatePolyMesh
{
private:
	void GetIndexedNormalsFromSpecifiedNormals( MNMesh* polyMesh, Matrix3 &meshTM_I_T, IndexedNormals &indexedNormals );
	void GetIndexedNormalsFromSpecifiedNormals( Mesh *triMesh, Matrix3 &meshTM_I_T, IndexedNormals &indexedNormals );

	void GetIndexedNormalsFromSmoothingGroups( MNMesh* polyMesh, Matrix3 &meshTM_I_T, std::vector<Abc::int32_t> &faceIndices, IndexedNormals &indexedNormals );
	void GetIndexedNormalsFromSmoothingGroups( Mesh *triMesh, Matrix3 &meshTM_I_T, std::vector<Abc::int32_t> &faceIndices, IndexedNormals &indexedNormals );

	void GetIndexedUVsFromChannel( MNMesh *polyMesh, int chanNum, IndexedUVs &indexedUVs );
	void GetIndexedUVsFromChannel( Mesh *triMesh, int chanNum, IndexedUVs &indexedUVs );

public:

    static void make_face_uv(Face *f, Point3 *tv);
    static BOOL CheckForFaceMap(Mtl* mtl, Mesh* mesh);

	void Save(std::map<std::string, bool>& mOptions, Mesh *triMesh, MNMesh* polyMesh, Matrix3& meshTM, Mtl* pMtl, int nMatId, bool bFirstFrame, materialsMergeStr* pMatMerge);
};


#endif