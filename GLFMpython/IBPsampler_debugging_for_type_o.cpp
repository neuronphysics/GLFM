#include "IBPsampler.h"
#include "GeneralFunctions.cpp"
#include "InferenceFunctions.cpp"

//*********************************INPUTS**************************//

#define input_X prhs[0]
#define input_C prhs[1]
#define input_Z prhs[2]
#define input_bias prhs[3]
#define input_s2Y prhs[4]
#define input_s2B prhs[5]
#define input_alpha prhs[6]
#define input_Nsim prhs[7]
#define input_maxK prhs[8]
#define input_missing prhs[9]

//*********************************OUTPUTS**************************//
#define output_Z plhs[0]
#define output_B plhs[1]
#define output_Theta plhs[2]

void mexFunction( int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[] ) {

    //..................CHECKING INPUTS AND OUTPUTS.............//
    /* Matrices are arranged per column */

    if (nrhs!=10) {
        mexErrMsgTxt("Invalid number of arguments\n");
    }

    const mwSize XNumDim = mxGetNumberOfDimensions(input_X);
    const mwSize ZNumDim = mxGetNumberOfDimensions(input_Z);
    const mwSize s2BNumDim = mxGetNumberOfDimensions(input_s2B);

    const mwSize* Xdim = mxGetDimensions(input_X);
    const mwSize* Zdim = mxGetDimensions(input_Z);
    const mwSize* Cdim = mxGetDimensions(input_C);

    int N = Xdim[0];
    int D = Xdim[1];
    int K = Zdim[1];
    //printf("C1 %d, C2 %d \n",Cdim[0], Cdim[1]);
    if (Cdim[1]!=D & Cdim[0]!=1){
        mexErrMsgTxt("Invalid number of dimensions for vector C\n");
        }

    double *X_dou = mxGetPr(input_X);
    char *C= mxArrayToString(input_C);
    double *Z_dou = mxGetPr(input_Z);

    //Inputs to the C function
    int bias = mxGetScalar(input_bias);
    double s2B = mxGetScalar(input_s2B);
    double s2Y = mxGetScalar(input_s2Y);
    double alpha = mxGetScalar(input_alpha);
    int maxK = mxGetScalar(input_maxK);
    int Nsim = mxGetScalar(input_Nsim);
    double missing = mxGetScalar(input_missing);

    gsl_matrix_view Zview = gsl_matrix_view_array(Z_dou, K,N);
    gsl_matrix *Zm = &Zview.matrix;
    gsl_matrix *Z= gsl_matrix_calloc(maxK,N);
    for (int i=0;i<N;i++){
        for (int k=0; k<K; k++){
        gsl_matrix_set (Z, k, i,gsl_matrix_get (Zm, k, i));
        }
    }

    gsl_matrix_view Xview = gsl_matrix_view_array(X_dou, D,N);
    gsl_matrix *X = &Xview.matrix;
    gsl_matrix **B=(gsl_matrix **) calloc(D,sizeof(gsl_matrix*));
    gsl_vector **theta=(gsl_vector **) calloc(D,sizeof(gsl_vector*));

    //...............BODY CODE.......................//
    //Starting C function
    for (int d=0; d<D; d++){
          C[d] = tolower(C[d]);//convert to lower case
          //printf("%c ",C[d]);
         }

    int R[D];
    double w[D], maxX[D];
    int maxR=1;
    gsl_vector_view Xd_view;
    for (int d=0; d<D; d++){
         Xd_view = gsl_matrix_row(X, d);
         maxX[d] = gsl_vector_max(&Xd_view.vector);
         R[d]=1;
         w[d]=1;
          switch(C[d]){
            case 'g':
                B[d] = gsl_matrix_alloc(maxK,1);
                break;
            case 'p':
                B[d] = gsl_matrix_alloc(maxK,1);
                w[d]=2/maxX[d];
                break;
            case 'n':
                B[d] = gsl_matrix_alloc(maxK,1);
                w[d]=2/maxX[d];
                break;
            case 'c':
                R[d]=(int)maxX[d];
                B[d] = gsl_matrix_alloc(maxK,R[d]);
                if (R[d]>maxR){maxR=R[d];}
                break;
            case 'o':
                R[d]=(int)maxX[d];
                B[d] = gsl_matrix_alloc(maxK,1);
                theta[d] = gsl_vector_alloc(R[d]);
                if (R[d]>maxR){maxR=R[d];}
                break;
         }
    }

  //...............Inference Function.......................//
   int Kest = IBPsampler_func (missing, X, C, Z, B, theta, R, w, maxR, bias, N, D, K, alpha, s2B, s2Y, maxK, Nsim);

   //...............SET OUTPUT POINTERS.......................//
    output_Z = mxCreateDoubleMatrix(Kest,N,mxREAL);
    double *pZ=mxGetPr(output_Z);

    int dimB[3]={D,Kest,maxR};
    output_B = mxCreateNumericArray(3,dimB,mxDOUBLE_CLASS,mxREAL);
    double *pB=mxGetPr(output_B);

    output_Theta = mxCreateDoubleMatrix(D, maxR,mxREAL);
    double *pT=mxGetPr(output_Theta);

    Zview = gsl_matrix_submatrix (Z, 0, 0, Kest, N);    
    for (int i=0;i<N;i++){
        for (int k=0; k<Kest; k++){
            pZ[Kest*i+k]=(&Zview.matrix)->data[k*N+i];
            //pgu[i]=xPLS[i];
        }
    }

    int idx_tmp;
    for (int d=0; d<D; d++){
        if (C[d] =='c') {
            idx_tmp = 1;
        } else {
            idx_tmp = R[d];
        }
        gsl_matrix_view Bd_view =  gsl_matrix_submatrix (B[d], 0, 0, Kest, idx_tmp);
        gsl_matrix *BT=gsl_matrix_alloc(idx_tmp,Kest);
        gsl_matrix_transpose_memcpy (BT, &Bd_view.matrix);;
        printf("CHECK\n");
        for (int i=0;i<Kest*maxR;i++){
            if (C[d]!='c' & i<Kest*idx_tmp){
                pB[D*i+d]=(BT)->data[i];
            }else if (C[d]=='c' & i<Kest*idx_tmp){
                pB[D*i+d]=(BT)->data[i];
            }
        }
        gsl_matrix_free(BT);
    }
    printf("B loaded\n");

    for (int d=0; d<D; d++){
        for (int i=0;i<maxR;i++){
            if (C[d]=='o' & i<R[d]){
                pT[D*i+d]=(theta[d])->data[i];
            }
        }
    }


    //..... Free memory.....//
    for (int d=0; d<D; d++){
        gsl_matrix_free(B[d]);
        gsl_vector_free(theta[d]);
    }
    gsl_matrix_free(Z);
    free(B);
    free(theta);

}

