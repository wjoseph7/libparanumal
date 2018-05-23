#include "acoustics.h"

acoustics_t *acousticsSetup(mesh_t *mesh, setupAide &newOptions, char* boundaryHeaderFileName){

  // OCCA build stuff
  char deviceConfig[BUFSIZ];
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  long int hostId = gethostid();

  long int* hostIds = (long int*) calloc(size,sizeof(long int));
  MPI_Allgather(&hostId,1,MPI_LONG,hostIds,1,MPI_LONG,MPI_COMM_WORLD);

  int deviceID = 0;
  for (int r=0;r<rank;r++) {
    if (hostIds[r]==hostId) deviceID++;
  }

  if (size==1) options.getArgs("DEVICE NUMBER" ,deviceID);

  // read thread model/device/platform from newOptions
  if(newOptions.compareArgs("THREAD MODEL", "CUDA")){
    sprintf(deviceConfig, "mode = CUDA, deviceID = %d",deviceID);
  }
  else if(newOptions.compareArgs("THREAD MODEL", "OpenCL")){
    int plat;
    newOptions.getArgs("PLATFORM NUMBER", plat);
    sprintf(deviceConfig, "mode = OpenCL, deviceID = %d, platformID = %d", deviceID, plat);
  }
  else if(newOptions.compareArgs("THREAD MODEL", "OpenMP")){
    sprintf(deviceConfig, "mode = OpenMP");
  }
  else{
    sprintf(deviceConfig, "mode = Serial");
  }
	
  acoustics_t *acoustics = (acoustics_t*) calloc(1, sizeof(acoustics_t));

  newOptions.getArgs("MESH DIMENSION", acoustics->dim);
  newOptions.getArgs("ELEMENT TYPE", acoustics->elementType);
  
  mesh->Nfields = (acoustics->dim==3) ? 4:3;
  acoustics->Nfields = mesh->Nfields;
  
  acoustics->mesh = mesh;

  dlong Ntotal = mesh->Nelements*mesh->Np*mesh->Nfields;
  acoustics->Nblock = (Ntotal+blockSize-1)/blockSize;
  
  hlong localElements = (hlong) mesh->Nelements;
  MPI_Allreduce(&localElements, &(acoustics->totalElements), 1, MPI_HLONG, MPI_SUM, MPI_COMM_WORLD);

  // viscosity
  int check;

  // compute samples of q at interpolation nodes
  mesh->q    = (dfloat*) calloc((mesh->totalHaloPairs+mesh->Nelements)*mesh->Np*mesh->Nfields,
				sizeof(dfloat));
  acoustics->rhsq = (dfloat*) calloc(mesh->Nelements*mesh->Np*mesh->Nfields,
				sizeof(dfloat));
  
  if (newOptions.compareArgs("TIME INTEGRATOR","LSERK4")){
    acoustics->resq = (dfloat*) calloc(mesh->Nelements*mesh->Np*mesh->Nfields,
		  		sizeof(dfloat));
  }

  if (newOptions.compareArgs("TIME INTEGRATOR","DOPRI5")){
    int NrkStages = 7;
    acoustics->rkq  = (dfloat*) calloc((mesh->totalHaloPairs+mesh->Nelements)*mesh->Np*mesh->Nfields,
          sizeof(dfloat));
    acoustics->rkrhsq = (dfloat*) calloc(NrkStages*mesh->Nelements*mesh->Np*mesh->Nfields,
          sizeof(dfloat));
    acoustics->rkerr  = (dfloat*) calloc((mesh->totalHaloPairs+mesh->Nelements)*mesh->Np*mesh->Nfields,
          sizeof(dfloat));

    acoustics->errtmp = (dfloat*) calloc(acoustics->Nblock, sizeof(dfloat));

    // Dormand Prince -order (4) 5 with PID timestep control
    int Nrk = 7;
    dfloat rkC[7] = {0.0, 0.2, 0.3, 0.8, 8.0/9.0, 1.0, 1.0};
    dfloat rkA[7*7] ={             0.0,             0.0,            0.0,          0.0,             0.0,       0.0, 0.0,
                                   0.2,             0.0,            0.0,          0.0,             0.0,       0.0, 0.0,
                              3.0/40.0,        9.0/40.0,            0.0,          0.0,             0.0,       0.0, 0.0,
                             44.0/45.0,      -56.0/15.0,       32.0/9.0,          0.0,             0.0,       0.0, 0.0,
                        19372.0/6561.0, -25360.0/2187.0, 64448.0/6561.0, -212.0/729.0,             0.0,       0.0, 0.0,
                         9017.0/3168.0,     -355.0/33.0, 46732.0/5247.0,   49.0/176.0, -5103.0/18656.0,       0.0, 0.0, 
                            35.0/384.0,             0.0,   500.0/1113.0,  125.0/192.0,  -2187.0/6784.0, 11.0/84.0, 0.0 };
    dfloat rkE[7] = {71.0/57600.0,  0.0, -71.0/16695.0, 71.0/1920.0, -17253.0/339200.0, 22.0/525.0, -1.0/40.0 }; 

    acoustics->Nrk = Nrk;
    acoustics->rkC = (dfloat*) calloc(acoustics->Nrk, sizeof(dfloat));
    acoustics->rkE = (dfloat*) calloc(acoustics->Nrk, sizeof(dfloat));
    acoustics->rkA = (dfloat*) calloc(acoustics->Nrk*acoustics->Nrk, sizeof(dfloat));

    memcpy(acoustics->rkC, rkC, acoustics->Nrk*sizeof(dfloat));
    memcpy(acoustics->rkE, rkE, acoustics->Nrk*sizeof(dfloat));
    memcpy(acoustics->rkA, rkA, acoustics->Nrk*acoustics->Nrk*sizeof(dfloat));
    
    acoustics->dtMIN = 1E-9; //minumum allowed timestep
    acoustics->ATOL = 1E-6;  //absolute error tolerance
    acoustics->RTOL = 1E-6;  //relative error tolerance
    acoustics->safe = 0.8;   //safety factor

    //error control parameters
    acoustics->beta = 0.05;
    acoustics->factor1 = 0.2;
    acoustics->factor2 = 10.0;


    acoustics->exp1 = 0.2 - 0.75*acoustics->beta;
    acoustics->invfactor1 = 1.0/acoustics->factor1;
    acoustics->invfactor2 = 1.0/acoustics->factor2;
    acoustics->facold = 1E-4;
    
  }

  // fix this later (initial conditions)
  for(dlong e=0;e<mesh->Nelements;++e){
    for(int n=0;n<mesh->Np;++n){
      dfloat t = 0;
      dfloat x = mesh->x[n + mesh->Np*e];
      dfloat y = mesh->y[n + mesh->Np*e];
      dfloat z = mesh->z[n + mesh->Np*e];

      dlong qbase = e*mesh->Np*mesh->Nfields + n;

      dfloat u = 0, v = 0, w = 0, r = 0;
      
      acousticsGaussianPulse(x, y, z, t, &r, &u, &v, &w);
      mesh->q[qbase+0*mesh->Np] = r;
      mesh->q[qbase+1*mesh->Np] = u;
      mesh->q[qbase+2*mesh->Np] = v;
      if(acoustics->dim==3)
	mesh->q[qbase+3*mesh->Np] = w;
    }
  }

  // set penalty parameter
  mesh->Lambda2 = 0.5;
  
  // set time step
  dfloat hmin = 1e9;
  for(dlong e=0;e<mesh->Nelements;++e){  

    for(int f=0;f<mesh->Nfaces;++f){
      dlong sid = mesh->Nsgeo*(mesh->Nfaces*e + f);
      dfloat sJ   = mesh->sgeo[sid + SJID];
      dfloat invJ = mesh->sgeo[sid + IJID];

      if(invJ<0) printf("invJ = %g\n", invJ);
      
      // sJ = L/2, J = A/2,   sJ/J = L/A = L/(0.5*h*L) = 2/h
      // h = 0.5/(sJ/J)
      
      dfloat hest = .5/(sJ*invJ);

      hmin = mymin(hmin, hest);
    }
  }

  // need to change cfl and defn of dt
  dfloat cfl = 0.5; // depends on the stability region size

  dfloat dtAdv  = hmin/((mesh->N+1.)*(mesh->N+1.));
  dfloat dt = cfl*dtAdv;
  
  // MPI_Allreduce to get global minimum dt
  MPI_Allreduce(&dt, &(mesh->dt), 1, MPI_DFLOAT, MPI_MIN, MPI_COMM_WORLD);
  
  //
  newOptions.getArgs("FINAL TIME", mesh->finalTime);

  mesh->NtimeSteps = mesh->finalTime/mesh->dt;
  if (newOptions.compareArgs("TIME INTEGRATOR","LSERK4")){
    mesh->dt = mesh->finalTime/mesh->NtimeSteps;
  }

  if (rank ==0) printf("dtAdv = %lg (before cfl), dt = %lg\n",
   dtAdv, dt);

  acoustics->frame = 0;
  // errorStep
  mesh->errorStep = 1000;

  if (rank ==0) printf("dt = %g\n", mesh->dt);

  // OCCA build stuff
  
  occa::kernelInfo kernelInfo;
  if(acoustics->dim==3)
    meshOccaSetup3D(mesh, deviceConfig, kernelInfo);
  else
    meshOccaSetup2D(mesh, deviceConfig, kernelInfo);

  //add boundary data to kernel info
  kernelInfo.addInclude(boundaryHeaderFileName);
 
  acoustics->o_q =
    mesh->device.malloc(mesh->Np*(mesh->totalHaloPairs+mesh->Nelements)*mesh->Nfields*sizeof(dfloat), mesh->q);

  acoustics->o_saveq =
    mesh->device.malloc(mesh->Np*(mesh->totalHaloPairs+mesh->Nelements)*mesh->Nfields*sizeof(dfloat), mesh->q);
  
  acoustics->o_rhsq =
    mesh->device.malloc(mesh->Np*mesh->Nelements*mesh->Nfields*sizeof(dfloat), acoustics->rhsq);

  cout << "TIME INTEGRATOR (" << newOptions.getArgs("TIME INTEGRATOR") << ")" << endl;
  
  if (newOptions.compareArgs("TIME INTEGRATOR","LSERK4")){
    acoustics->o_resq =
      mesh->device.malloc(mesh->Np*mesh->Nelements*mesh->Nfields*sizeof(dfloat), acoustics->resq);
  }

  if (newOptions.compareArgs("TIME INTEGRATOR","DOPRI5")){
    printf("setting up DOPRI5\n");
    int NrkStages = 7;
    acoustics->o_rkq =
      mesh->device.malloc(mesh->Np*(mesh->totalHaloPairs+mesh->Nelements)*mesh->Nfields*sizeof(dfloat), acoustics->rkq);
    acoustics->o_rkrhsq =
      mesh->device.malloc(NrkStages*mesh->Np*mesh->Nelements*mesh->Nfields*sizeof(dfloat), acoustics->rkrhsq);
    acoustics->o_rkerr =
      mesh->device.malloc(mesh->Np*(mesh->totalHaloPairs+mesh->Nelements)*mesh->Nfields*sizeof(dfloat), acoustics->rkerr);
  
    acoustics->o_errtmp = mesh->device.malloc(acoustics->Nblock*sizeof(dfloat), acoustics->errtmp);

    acoustics->o_rkA = mesh->device.malloc(acoustics->Nrk*acoustics->Nrk*sizeof(dfloat), acoustics->rkA);
    acoustics->o_rkE = mesh->device.malloc(  acoustics->Nrk*sizeof(dfloat), acoustics->rkE);
  }

  
  if(mesh->totalHaloPairs>0){
    // temporary DEVICE buffer for halo (maximum size Nfields*Np for dfloat)
    mesh->o_haloBuffer =
      mesh->device.malloc(mesh->totalHaloPairs*mesh->Np*mesh->Nfields*sizeof(dfloat));

    // MPI send buffer
    acoustics->haloBytes = mesh->totalHaloPairs*mesh->Np*acoustics->Nfields*sizeof(dfloat);
    occa::memory o_sendBuffer = mesh->device.mappedAlloc(acoustics->haloBytes, NULL);
    occa::memory o_recvBuffer = mesh->device.mappedAlloc(acoustics->haloBytes, NULL);
    acoustics->o_haloBuffer = mesh->device.malloc(acoustics->haloBytes);
    acoustics->sendBuffer = (dfloat*) o_sendBuffer.getMappedPointer();
    acoustics->recvBuffer = (dfloat*) o_recvBuffer.getMappedPointer();
  }

  //  p_RT, p_rbar, p_ubar, p_vbar
  // p_half, p_two, p_third, p_Nstresses
  
  kernelInfo.addDefine("p_Nfields", mesh->Nfields);
  const dfloat p_one = 1.0, p_two = 2.0, p_half = 1./2., p_third = 1./3., p_zero = 0;

  kernelInfo.addDefine("p_two", p_two);
  kernelInfo.addDefine("p_one", p_one);
  kernelInfo.addDefine("p_half", p_half);
  kernelInfo.addDefine("p_third", p_third);
  kernelInfo.addDefine("p_zero", p_zero);
  
  int maxNodes = mymax(mesh->Np, (mesh->Nfp*mesh->Nfaces));
  kernelInfo.addDefine("p_maxNodes", maxNodes);

  int NblockV = 1024/mesh->Np; // works for CUDA
  kernelInfo.addDefine("p_NblockV", NblockV);

  int NblockS = 1024/maxNodes; // works for CUDA
  kernelInfo.addDefine("p_NblockS", NblockS);

  int cubMaxNodes = mymax(mesh->Np, (mesh->intNfp*mesh->Nfaces));
  kernelInfo.addDefine("p_cubMaxNodes", cubMaxNodes);
  int cubMaxNodes1 = mymax(mesh->Np, (mesh->intNfp));
  kernelInfo.addDefine("p_cubMaxNodes1", cubMaxNodes1);

  kernelInfo.addDefine("p_Lambda2", 0.5f);

  kernelInfo.addDefine("p_blockSize", blockSize);


  kernelInfo.addParserFlag("automate-add-barriers", "disabled");

  // set kernel name suffix
  char *suffix;
  
  if(acoustics->elementType==TRIANGLES)
    suffix = strdup("Tri2D");
  if(acoustics->elementType==QUADRILATERALS)
    suffix = strdup("Quad2D");
  if(acoustics->elementType==TETRAHEDRA)
    suffix = strdup("Tet3D");
  if(acoustics->elementType==HEXAHEDRA)
    suffix = strdup("Hex3D");

  char fileName[BUFSIZ], kernelName[BUFSIZ];

  // kernels from volume file
  sprintf(fileName, DACOUSTICS "/okl/acousticsVolume%s.okl", suffix);
  sprintf(kernelName, "acousticsVolume%s", suffix);

  printf("fileName=[ %s ] \n", fileName);
  printf("kernelName=[ %s ] \n", kernelName);
  
  acoustics->volumeKernel =  mesh->device.buildKernelFromSource(fileName, kernelName, kernelInfo);

  // kernels from surface file
  sprintf(fileName, DACOUSTICS "/okl/acousticsSurface%s.okl", suffix);
  sprintf(kernelName, "acousticsSurface%s", suffix);
  
  acoustics->surfaceKernel = mesh->device.buildKernelFromSource(fileName, kernelName, kernelInfo);

  // kernels from update file
  acoustics->updateKernel =
    mesh->device.buildKernelFromSource(DACOUSTICS "/okl/acousticsUpdate.okl",
				       "acousticsUpdate",
				       kernelInfo);

  acoustics->rkUpdateKernel =
    mesh->device.buildKernelFromSource(DACOUSTICS "/okl/acousticsUpdate.okl",
				       "acousticsRkUpdate",
				       kernelInfo);
  acoustics->rkStageKernel =
    mesh->device.buildKernelFromSource(DACOUSTICS "/okl/acousticsUpdate.okl",
				       "acousticsRkStage",
				       kernelInfo);

  acoustics->rkErrorEstimateKernel =
    mesh->device.buildKernelFromSource(DACOUSTICS "/okl/acousticsUpdate.okl",
				       "acousticsErrorEstimate",
				       kernelInfo);

  // fix this later
  mesh->haloExtractKernel =
    mesh->device.buildKernelFromSource(DHOLMES "/okl/meshHaloExtract3D.okl",
				       "meshHaloExtract3D",
				       kernelInfo);

  return acoustics;
}