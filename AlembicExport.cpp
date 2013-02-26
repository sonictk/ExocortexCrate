// alembicPlugin
// Initial code generated by Softimage SDK Wizard
// Executed Fri Aug 19 09:14:49 UTC+0200 2011 by helge
// 
// Tip: You need to compile the generated code before you can load the plug-in.
// After you compile the plug-in, you can load it by clicking Update All in the Plugin Manager.
#include "stdafx.h"
#include "arnoldHelpers.h" 

using namespace XSI; 
using namespace MATH; 

#include "AlembicLicensing.h"

#include "AlembicWriteJob.h"
#include "AlembicPoints.h"
#include "AlembicCurves.h"
#include "CommonProfiler.h"
#include "CommonMeshUtilities.h"
#include "CommonUtilities.h"
#include "CommonSceneGraph.h"

CStatus exportCommandImp( CRef& in_ctxt );

ESS_CALLBACK_START(alembic_export_jobs_Init,CRef&)

	Context ctxt( in_ctxt );
	Command oCmd;
	oCmd = ctxt.GetSource();
	oCmd.PutDescription(L"");
	oCmd.EnableReturnValue(true);

	ArgumentArray oArgs;
	oArgs = oCmd.GetArguments();
	oArgs.Add(L"exportjobs");
	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_export_jobs_Execute,CRef&)
   return exportCommandImp(in_ctxt);
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_export_Init,CRef&)

	Context ctxt( in_ctxt );
	Command oCmd;
	oCmd = ctxt.GetSource();
	oCmd.PutDescription(L"");
	oCmd.EnableReturnValue(true);

	ArgumentArray oArgs;
	oArgs = oCmd.GetArguments();
	oArgs.Add(L"exportjobs");
	return CStatus::OK;
ESS_CALLBACK_END


ESS_CALLBACK_START(alembic_export_Execute,CRef&)
   ESS_LOG_WARNING("The alembic_export command is deprecated. Please use alembic_export_jobs instead.");
   return exportCommandImp(in_ctxt);
ESS_CALLBACK_END

CStatus exportCommandImp( CRef& in_ctxt )
{
   Context ctxt( in_ctxt );
	CValueArray args = ctxt.GetAttribute(L"Arguments");

	//FORCE_CRASH_INVALID_ACCESS_VIOLATION; used for testing whether error reporting works.
	ESS_PROFILE_SCOPE("alembic_export_Execute");

   // get all of the jobs, and split them
   CString jobString = args[0].GetAsText();
   if(jobString.IsEmpty())
   {
	 CValue objectsAsValue = args[1];
   	CRefArray objects = objectsAsValue;
  
      if(objects.GetCount() == 0)
      {
         // use the selection
         objects = Application().GetSelection().GetArray();
         if(objects.GetCount() == 0)
         {
            Application().LogMessage(L"[ExocortexAlembic] No objects specified.",siErrorMsg);
            return CStatus::InvalidArgument;
         }
      }
      jobString += L"objects=";
      for(LONG i=0;i<objects.GetCount();i++)
      {
         if(i>0)
            jobString += L",";
         jobString += ProjectItem(objects[i]).GetFullName();
      }

      // let's setup the property
      CustomProperty settings = (CustomProperty) Application().GetActiveSceneRoot().AddProperty(L"alembic_export_settings");

      // inspect it
      CValueArray inspectArgs(5);
      CValue inspectResult;
      inspectArgs[0] = settings.GetFullName();
      inspectArgs[1] = L"";
      inspectArgs[2] = L"Export Settings";
      inspectArgs[3] = siModal;
      inspectArgs[4] = false;
      Application().ExecuteCommand(L"InspectObj",inspectArgs,inspectResult);
      
      // prepare for deletion
      inspectArgs.Resize(1);
      inspectArgs[0] = settings.GetFullName();
      if((bool)inspectResult)
      {
         Application().ExecuteCommand(L"DeleteObj",inspectArgs,inspectResult);
         return CStatus::Abort;
      }

      // retrieve the options
      jobString += L";in="+settings.GetParameterValue(L"frame_in").GetAsText();
      jobString += L";out="+settings.GetParameterValue(L"frame_out").GetAsText();
      jobString += L";step="+settings.GetParameterValue(L"frame_step").GetAsText();
      jobString += L";substep="+settings.GetParameterValue(L"frame_substep").GetAsText();
      LONG normalMode = settings.GetParameterValue(L"normals");
      bool transformCache = settings.GetParameterValue(L"transformcache");
      jobString += L";normals="+CValue(normalMode == 2l).GetAsText();
      if(transformCache)
      {
         jobString += L";transformcache=true";
         jobString += L";uvs=false";
         jobString += L";facesets=false";
         jobString += L";bindpose=false";
         //jobString += L";dynamictopology=false";
      }
      else if(normalMode == 0)
      {
         jobString += L";purepointcache=true";
         jobString += L";uvs=false";
         jobString += L";facesets=false";
         jobString += L";bindpose=false";
         //jobString += L";dynamictopology=false";
      }
      else
      {
         jobString += L";uvs="+settings.GetParameterValue(L"uvs").GetAsText();
         jobString += L";facesets="+settings.GetParameterValue(L"facesets").GetAsText();
	      jobString += L";bindpose="+settings.GetParameterValue(L"bindpose").GetAsText();
	      //jobString += L";dynamictopology="+settings.GetParameterValue(L"dtopology").GetAsText();
      }
      jobString += L";guidecurves="+settings.GetParameterValue(L"guidecurves").GetAsText();
 
      
      LONG transformMode = settings.GetParameterValue(L"transforms");
      if( transformMode == 0 ){//flatten hierarchy
         jobString += L";transformHierarchy=flat";
      }
      else if( transformMode == 1){//full hierarchy
         jobString += L";transformHierarchy=full";
      }
      else if( transformMode == 2){//bake in
         jobString += L";transformHierarchy=bake";
      }

      Application().ExecuteCommand(L"DeleteObj",inspectArgs,inspectResult);
   }

   CStringArray jobs = jobString.Split(L"|");
   std::vector<AlembicWriteJob*> jobPtrs;

   double minFrame = 1000000.0;
   double maxFrame = -1000000.0;
   double maxSteps = 1;
   double maxSubsteps = 1;

   // for each job, check the arguments
   for(LONG i=0;i<jobs.GetCount();i++)
   {
      double frameIn = 1.0;
      double frameOut = 1.0;
      double frameSteps = 1.0;
      double frameSubSteps = 1.0;
      CString filename;
      bool transformCache = false;
      bool purepointcache = false;
      bool normals = true;
      bool uvs = true;
      bool facesets = true;
	  bool bindpose = true;
      bool dynamictopology = false;
      bool globalspace = false;
      bool flattenhierarchy = true;
      bool guidecurves = false;
	  bool geomApproxSubD = false;
      //CRefArray objects;

     std::vector<std::string> objects;

      // process all tokens of the job
      CStringArray tokens = jobs[i].Split(L";");
      for(LONG j=0;j<tokens.GetCount();j++)
      {
         CStringArray valuePair = tokens[j].Split(L"=");
         if(valuePair.GetCount()!=2)
         {
            Application().LogMessage(L"[ExocortexAlembic] Skipping invalid token: "+tokens[j],siWarningMsg);
            continue;
         }

         if(valuePair[0].IsEqualNoCase(L"in")){
            frameIn = (double)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"out")){
            frameOut = (double)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"step")){
            frameSteps = (double)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"substep")){
            frameSubSteps = (double)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"normals")){
            normals = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"uvs")){
            uvs = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"facesets")){
            facesets = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"bindpose")){
            bindpose = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"transformcache")){
            transformCache = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"purepointcache")){
            purepointcache = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"dynamictopology")){
            dynamictopology = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"globalspace")){
            globalspace = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"flattenhierarchy")){
            flattenhierarchy = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"transformHierarchy")){
            if(valuePair[1].IsEqualNoCase(L"full")){
               flattenhierarchy = false;
               globalspace = false;
            }
            else if(valuePair[1].IsEqualNoCase(L"flat")){
               flattenhierarchy = true;
               globalspace = false;
            }
            else if(valuePair[1].IsEqualNoCase(L"bake")){
               flattenhierarchy = true;
               globalspace = true;
            }
            else{
               Application().LogMessage(L"[ExocortexAlembic] Incorrect transformHierarchy parameter: " + valuePair[1], siWarningMsg);
            }
         }
         else if(valuePair[0].IsEqualNoCase(L"guidecurves"))
         {
            guidecurves = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"filename")){
            filename = CValue(valuePair[1]).GetAsText();
         }
         else if(valuePair[0].IsEqualNoCase(L"geomApproxSubD")){
            geomApproxSubD = (bool)CValue(valuePair[1]);
         }
         else if(valuePair[0].IsEqualNoCase(L"objects"))
         {
            // try to find each object
            CStringArray objectStrings = valuePair[1].Split(L",");
            for(LONG k=0;k<objectStrings.GetCount();k++)
            {
               CRef objRef;
               objRef.Set(objectStrings[k]);
               if(!objRef.IsValid())
               {
                  Application().LogMessage(L"[ExocortexAlembic] Skipping object 'L"+objectStrings[k]+"', not found.",siWarningMsg);
                  continue;
               }
               objects.push_back(objectStrings[k].GetAsciiString());
            }
         }
         else
         {
            Application().LogMessage(L"[ExocortexAlembic] Skipping invalid token: "+tokens[j],siWarningMsg);
            continue;
         }
      }

      // check if we have incompatible subframes
      if(maxSubsteps > 1.0 && frameSubSteps > 1.0)
      {
         if(maxSubsteps > frameSubSteps)
         {
            double part = maxSubsteps / frameSubSteps;
            if(abs(part - floor(part)) > 0.001)
            {
               Application().LogMessage(L"[ExocortexAlembic] You cannot combine substeps "+CString(frameSubSteps)+L" and "+CString(maxSubsteps)+L" in one export. Aborting.",siErrorMsg);
               return CStatus::InvalidArgument;
            }
         }
         else if(frameSubSteps > maxSubsteps )
         {
            double part = frameSubSteps / maxSubsteps;
            if(abs(part - floor(part)) > 0.001)
            {
               Application().LogMessage(L"[ExocortexAlembic] You cannot combine substeps "+CString(maxSubsteps)+L" and "+CString(frameSubSteps)+L" in one export. Aborting.",siErrorMsg);
               return CStatus::InvalidArgument;
            }
         }
      }

      // remember the min and max values for the frames
      if(frameIn < minFrame) minFrame = frameIn;
      if(frameOut > maxFrame) maxFrame = frameOut;
      if(frameSteps > maxSteps) maxSteps = frameSteps;
      if(frameSteps > 1.0) frameSubSteps = 1.0;
      if(frameSubSteps > maxSubsteps) maxSubsteps = frameSubSteps;

      // check if we have a filename
      if(filename.IsEmpty())
      {
         // let's see if we are in interactive mode
         if(Application().IsInteractive())
         {
            CComAPIHandler toolkit;
            toolkit.CreateInstance(L"XSI.UIToolkit");
            CComAPIHandler filebrowser(toolkit.GetProperty(L"FileBrowser"));
            filebrowser.PutProperty(L"InitialDirectory",Application().GetActiveProject().GetPath());
            filebrowser.PutProperty(L"Filter",L"Alembic Files(*.abc)|*.abc||");
            CValue returnVal;
            filebrowser.Call(L"ShowSave",returnVal);
            filename = filebrowser.GetProperty(L"FilePathName").GetAsText();            
         }
         else
         {
            Application().LogMessage(L"[ExocortexAlembic] No filename specified.",siErrorMsg);
            for(size_t k=0;k<jobPtrs.size();k++)
               delete(jobPtrs[k]);
            return CStatus::InvalidArgument;
         }
      }

      if(filename.IsEmpty() || !validate_filename_location(filename.GetAsciiString()))
      {
         Application().LogMessage(L"[ExocortexAlembic] cannot write file " + filename,siErrorMsg);
         for(size_t k=0;k<jobPtrs.size();k++)
            delete(jobPtrs[k]);
         return CStatus::Abort;
      }

      // construct the frames
      CDoubleArray frames;
      for(double frame=frameIn; frame<=frameOut; frame+=frameSteps / frameSubSteps)
         frames.Add(frame);

      if(globalspace){
         flattenhierarchy = true;
      }

      AlembicWriteJob * job = new AlembicWriteJob(filename, objects, frames);
      job->SetOption(L"transformCache",transformCache);
      job->SetOption(L"exportNormals",normals);
      job->SetOption(L"exportUVs",uvs);
      job->SetOption(L"exportFaceSets",facesets);
	   job->SetOption(L"exportBindPose",bindpose);
      job->SetOption(L"exportPurePointCache",purepointcache);
      job->SetOption(L"exportDynamicTopology",true);
      job->SetOption(L"indexedNormals",true);
      job->SetOption(L"indexedUVs",true);
      job->SetOption(L"globalSpace",globalspace);
      job->SetOption(L"flattenHierarchy",flattenhierarchy);
      job->SetOption(L"guideCurves",guidecurves);
	  job->SetOption(L"geomApproxSubD",geomApproxSubD);

      // check if the job is satifsied
      if(job->PreProcess() != CStatus::OK)
      {
         Application().LogMessage(L"[ExocortexAlembic] Job skipped. Not satisfied.",siErrorMsg);
         delete(job);
         continue;
      }

      // push the job to our registry
      Application().LogMessage(L"[ExocortexAlembic] Using WriteJob:"+jobs[i]);
      jobPtrs.push_back(job);
   }

   // compute the job count
   ULONG jobCount = 0;   
   ULONG objectCount = 0;
   for(size_t i=0;i<jobPtrs.size();i++) {
      jobCount += (ULONG)jobPtrs[i]->GetNbObjects() * (ULONG)jobPtrs[i]->GetFrames().size();
	  objectCount += (ULONG)jobPtrs[i]->GetNbObjects();
   }



   ProgressBar prog;
   prog = Application().GetUIToolkit().GetProgressBar();
   prog.PutCaption(L"Exporting "+CString(jobCount)+L" frames from " + CString(objectCount) + " objects...");
   prog.PutMinimum(0);
   prog.PutMaximum(jobCount);
   prog.PutValue(0);
   prog.PutCancelEnabled(true);
   prog.PutVisible(true);
	
   // now, let's run through all frames, and process the jobs
   CValueArray setFrameArgs;
   CValue setFrameResult;
   for(double frame = minFrame; frame<=maxFrame; frame += maxSteps / maxSubsteps)
   {
      setFrameArgs.Resize(2);
      setFrameArgs[0] = L"PlayControl.Current";
      setFrameArgs[1] = frame;
      Application().ExecuteCommand(L"SetValue",setFrameArgs,setFrameResult);

      setFrameArgs.Resize(1);
      setFrameArgs[0] = frame;
      Application().ExecuteCommand(L"Refresh",setFrameArgs,setFrameResult);

      bool canceled = false;
      for(size_t i=0;i<jobPtrs.size();i++)
      {
         CStatus status = jobPtrs[i]->Process(frame);
         if(status == CStatus::OK)
            prog.Increment((LONG)jobPtrs[i]->GetNbObjects());
         else if(status != CStatus::False)
         {
            for(size_t k=0;k<jobPtrs.size();k++)
               delete(jobPtrs[k]);
            return status;
         }

         if(prog.IsCancelPressed())
         {
            canceled = true;
            break;
         }
      }
      if(canceled)
         break;
   }

   prog.PutVisible(false);

   // delete all jobs
   for(size_t k=0;k<jobPtrs.size();k++)
      delete(jobPtrs[k]);

   // remove all known archives
   deleteAllArchives();

   return CStatus::OK;
}


ESS_CALLBACK_START(alembic_export_settings_Define,CRef&)
	Context ctxt( in_ctxt );
	CustomProperty oCustomProperty;
	Parameter oParam;
	oCustomProperty = ctxt.GetSource();

   // get the current frame in an out
   CValueArray cmdArgs(1);
   CValue cmdReturnVal;
   cmdArgs[0] = L"PlayControl.In";
   Application().ExecuteCommand(L"GetValue",cmdArgs,cmdReturnVal);
   oCustomProperty.AddParameter(L"frame_in",CValue::siInt4,siPersistable,L"",L"",floorf(float(cmdReturnVal)+0.5f),-1000000,1000000,1,100,oParam);
   cmdArgs[0] = L"PlayControl.Out";
   Application().ExecuteCommand(L"GetValue",cmdArgs,cmdReturnVal);
   oCustomProperty.AddParameter(L"frame_out",CValue::siInt4,siPersistable,L"",L"",floorf(float(cmdReturnVal)+0.5f),-1000000,1000000,1,100,oParam);
   oCustomProperty.AddParameter(L"frame_step",CValue::siInt4,siPersistable,L"",L"",1,-1000000,1000000,1,5,oParam);
   oCustomProperty.AddParameter(L"frame_substep",CValue::siInt4,siPersistable,L"",L"",1,-1000000,1000000,1,5,oParam);

	oCustomProperty.AddParameter(L"normals",CValue::siInt4,siPersistable,L"",L"",2,0,10,0,10,oParam);
   oCustomProperty.AddParameter(L"uvs",CValue::siBool,siPersistable,L"",L"",1,0,1,0,1,oParam);
   oCustomProperty.AddParameter(L"facesets",CValue::siBool,siPersistable,L"",L"",1,0,1,0,1,oParam);
   oCustomProperty.AddParameter(L"bindpose",CValue::siBool,siPersistable,L"",L"",1,0,1,0,1,oParam);
   //oCustomProperty.AddParameter(L"globalspace",CValue::siBool,siPersistable,L"",L"",0,0,1,0,1,oParam);
   //oCustomProperty.AddParameter(L"dtopology",CValue::siBool,siPersistable,L"",L"",0,0,1,0,1,oParam);
   oCustomProperty.AddParameter(L"guidecurves",CValue::siBool,siPersistable,L"",L"",0,0,1,0,1,oParam);
   oCustomProperty.AddParameter(L"transformcache",CValue::siBool,siPersistable,L"",L"",0,0,1,0,1,oParam);
   oCustomProperty.AddParameter(L"transforms",CValue::siInt4,siPersistable,L"",L"",0,0,10,0,10,oParam);

	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_export_settings_DefineLayout,CRef&)
	Context ctxt( in_ctxt );
	PPGLayout oLayout;
	PPGItem oItem;
	oLayout = ctxt.GetSource();
	oLayout.Clear();

   oLayout.AddGroup(L"Animation");
   oLayout.AddItem(L"frame_in",L"In");
   oLayout.AddItem(L"frame_out",L"Out");
   oLayout.AddItem(L"frame_step",L"Frame-Steps");
   oLayout.AddItem(L"frame_substep",L"Sub-Steps");
   oLayout.EndGroup();

   CValueArray normalItems(6);
   oLayout.AddGroup(L"Geometry");
   normalItems[0] = L"Point Cache (No Surface)";
   normalItems[1] = (LONG) 0l;
   normalItems[2] = L"Just Surface (No Normals)";
   normalItems[3] = (LONG) 1l;
   normalItems[4] = L"Surface + Normals (For Interchange)";
   normalItems[5] = (LONG) 2l;
   oLayout.AddEnumControl(L"normals",normalItems,L"Mesh Topology");
   oLayout.AddItem(L"uvs",L"UVs");
   oLayout.AddItem(L"facesets",L"Clusters");
   oLayout.AddItem(L"bindpose",L"Envelope BindPose");
   //oLayout.AddItem(L"dtopology",L"Dynamic Topology");
   oLayout.AddItem(L"guidecurves",L"Guide Curves");
   //oLayout.AddItem(L"globalspace",L"Use Global Space");
   oLayout.EndGroup();
   oLayout.AddItem(L"transformcache",L"Cache Transforms Only");
   
   CValueArray transformItems(6);
   transformItems[0] = L"Flatten Hierarchy";
   transformItems[1] = (LONG) 0;
   transformItems[2] = L"Full Hierarchy";
   transformItems[3] = (LONG) 1;
   transformItems[4] = L"Bake Into Geometry";
   transformItems[5] = (LONG) 2;
   oLayout.AddEnumControl(L"transforms", transformItems, L"Transforms");

	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_export_settings_PPGEvent,const CRef&)
	PPGEventContext ctxt( in_ctxt ) ;
	if ( ctxt.GetEventID() == PPGEventContext::siParameterChange )
	{
		Parameter param = ctxt.GetSource() ;	
		CString paramName = param.GetScriptName() ; 
      if(paramName.IsEqualNoCase(L"normals"))
      {
         bool enable = LONG(param.GetValue()) > 0;
         Property prop(param.GetParent());
         Parameter(prop.GetParameters().GetItem(L"uvs")).PutCapabilityFlag(siReadOnly,!enable);
         Parameter(prop.GetParameters().GetItem(L"facesets")).PutCapabilityFlag(siReadOnly,!enable);
         Parameter(prop.GetParameters().GetItem(L"bindpose")).PutCapabilityFlag(siReadOnly,!enable);
         //Parameter(prop.GetParameters().GetItem(L"dtopology")).PutCapabilityFlag(siReadOnly,!enable);
      }
	}
   else if (ctxt.GetEventID() == PPGEventContext::siButtonClicked)
   {
      CString buttonName = ctxt.GetAttribute(L"Button");
   }

	return CStatus::OK ;
ESS_CALLBACK_END