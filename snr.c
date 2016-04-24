/*
 * Signal differences measurements.
 *
 * Compile with:
 *
 *   gcc snr.c -o snr -lfftw3 -lm
 *
 * gse. 2011.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include <getopt.h>

#define BLOCK_SIZE 352*288+((352/2)*(288/2))*2

void help(void) {
  printf("snr \n");
  printf(" --type={[uchar]|ushort}\n");
  printf(" --peak={[255]|65535}\n");
  printf(" --file_A=First file to compare to\n");
  printf(" --file_B=the scond one.\n");
  printf(" --block_size=<block size in bytes=[%d]>\n",BLOCK_SIZE);
  printf(" --FFF={yes|[no]}\n");
  printf("\n");
  printf("Compute:\n");
  printf("               N-1\n");
  printf("   Energy(A) = Sum A[i]^2\n");
  printf("               i=0\n");
  printf("\n");
  printf("               N-1\n");
  printf("   Energy(B) = Sum B[i]^2\n");
  printf("               i=0\n");
  printf("\n");
  printf("         Energy(A)\n");
  printf("   SNR = ---------              (Signal-to-Noise Ratio)\n");
  printf("         Energy(B)\n");
  printf("\n");
  printf("          1  N-1\n");
  printf("   MSE = --- Sum (A[i]-B[i])^2  (Mean Squared Error)\n");
  printf("          N  i=0\n");
  printf("\n");
  printf("   RMSE = sqrt(MSE)             (Root Mean Squared Error)\n");
  printf("\n");
  printf("   SNR[dB] = 10 log SNR\n");
  printf("\n");
  printf("           peak^2 \n");
  printf("   PSNR = --------              (Peak SNR)\n");
  printf("             MSE \n");
  printf("\n");
  printf("   PSNR[dB] = 10 log PSNR\n");
  printf("\n");
  printf("between file_A and file_B\n");
}

int main (int argc, char *argv[]) {

  char *type = "uchar";

  FILE *file_A, *file_B;
  int i, n, counter=0;
  double mse, rmse, snr, snr_db, psnr, psnr_db;
  int peak = 255;
  int block_size = BLOCK_SIZE;
  int FFT = 0;

  int c;

  printf("%d\n",argc);
  if(argc==1) {
    help();
    exit(1);
  }

  while(1) {
    static struct option long_options[] = {
      {"block_size", required_argument, 0, 'b'},
      {"file_A",     required_argument, 0, 'A'},
      {"file_B",     required_argument, 0, 'B'},
      {"peak",       required_argument, 0, 'p'},
      {"type",       required_argument, 0, 't'},
      {"FFT",        no_argument,       0, 'F'},
      {"help",       no_argument,       0, '?'},
      {0, 0, 0, 0}
    };

    int option_index = 0;
    
    c = getopt_long(argc, argv,
		    "t:p:A:B:b:F?",
		    long_options, &option_index);
    
    if(c==-1) {
      /* Ya no hay más opciones. */
      break;
    }
    
    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
	break;
      printf("option %s", long_options[option_index].name);
      if (optarg)
	printf(" with arg %s", optarg);
      printf ("\n");
      break;
      
    case 't':
      type = optarg;
      printf("%s: type = %s\n", argv[0], type);
      break;
      
    case 'p':
      peak = atoi(optarg);
      printf("%s: peak = %d\n", argv[0], peak);
      break;
      
    case 'A':
      file_A=fopen(optarg,"rb");
      if(!file_A) {
	printf("%s: unable to open input file \"%s\"\n",argv[0],optarg);
	return 1;
      }
      break;
      
    case 'B':
      file_B=fopen(optarg,"rb");
      if(!file_B) {
	printf("%s: unable to open input file \"%s\"\n",argv[0],optarg);
	return 2;
      }
      break;
      
    case 'b':
      block_size = atoi(optarg);
      printf("%s: block_size = %d\n", argv[0], block_size);
      break;

    case 'F':
      FFT = 1;
      printf("%s: Fast Fourier Transform = yes\n", argv[0]);
      break;

    case '?':
      help();
      exit(1);

    default:
      abort ();
    }
  }

  if (!strcmp(type,"uchar")) {
    unsigned char *buf_A = (unsigned char *)malloc(block_size*sizeof(unsigned char));
    unsigned char *buf_B = (unsigned char *)malloc(block_size*sizeof(unsigned char));
    int block;
    printf("%s: data type: unsigned char\n",argv[0]);
    if(FFT) {
      double *signal_A = (double *)fftw_malloc(block_size*sizeof(double));
      double *signal_B = (double *)fftw_malloc(block_size*sizeof(double));
      fftw_complex *fourier_A =
	(fftw_complex *)fftw_malloc(block_size*sizeof(fftw_complex));
      fftw_complex *fourier_B =
	(fftw_complex *)fftw_malloc(block_size*sizeof(fftw_complex));
      fftw_plan f_A = fftw_plan_dft_r2c_1d(block_size, signal_A, fourier_A, 0);
      fftw_plan f_B = fftw_plan_dft_r2c_1d(block_size, signal_B, fourier_B, 0);
      double *spectrum_A = (double *)malloc(block_size/2*sizeof(double));
      double *spectrum_B = (double *)malloc(block_size/2*sizeof(double));
      /*unsigned*/double energy_A=0, energy_B=0, energy_error=0;
      printf("%s: performing the FFT!\n",argv[0]);
      for(block = 0;; block++) {
	double local_energy_error = energy_error, local_energy_A = energy_A;
	int local_counter = counter;
	int r_A = fread(buf_A,sizeof(unsigned char),block_size,file_A);
	int r_B = fread(buf_B,sizeof(unsigned char),block_size,file_B);
	int min = r_B;
	if(r_A < r_B) min = r_A;
	if(min==0) break;
	{
	  int i;
	  for(i=0; i<min; i++) {
	    signal_A[i] = buf_A[i];
	  }
	}
	fftw_execute(f_A);
	{
	  int i;
	  for(i=0; i<min/2; i++) {
	    spectrum_A[i] = sqrt(fourier_A[i][0]*fourier_A[i][0] + fourier_A[i][1]*fourier_A[i][1]);
	  }
	}
	{
	  int i;
	  for(i=0; i<min; i++) {
	    signal_B[i] = buf_B[i];
	  }
	}
	fftw_execute(f_B);
	{
	  int i;
	  for(i=0; i<min/2; i++) {
	  spectrum_B[i] = sqrt(fourier_B[i][0]*fourier_B[i][0] + fourier_B[i][1]*fourier_B[i][1]);
	  }
	}
	{
	  int i;
	  double a,b,ab;
	  for(i=0; i<min/2; i++) {
	    a=spectrum_A[i];
	    b=spectrum_B[i];
	    energy_A += a*a;
	    energy_B += b*b;
	    ab = a - b;
	    energy_error += ab*ab;
	    counter++;
	  }
	}
	if(energy_error) {
	  local_energy_error = energy_error - local_energy_error;
	  local_counter = counter - local_counter;
	  local_energy_A = energy_A - local_energy_A;
	  mse=local_energy_error/(double)(local_counter);
	  psnr=(double)(peak)*(double)(peak)/mse;
	  psnr_db=10.0*log10(psnr);
	  rmse=sqrt(mse);
	  snr=local_energy_A/local_energy_error;
	  snr_db=10.0*log10(snr);
	} else {
	  snr_db=1.0;
	  psnr_db=1.0;
	  /*printf("SNR infinito !!!\n");
	    return 1;*/
	}
	fprintf(stderr,"%3d\t%f\n", block, psnr_db);
      }

      if(energy_error) {
	mse=(double)(energy_error)/(double)(counter);
	psnr=(double)(peak)*(double)(peak)/mse;
	psnr_db=10.0*log10(psnr);
	rmse=sqrt(mse);
	snr=(double)(energy_A)/(double)(energy_error);
	snr_db=10.0*log10(snr);
      } else {
	mse=0.0;
	psnr=1.0;
	psnr_db=1.0;
	rmse=0.0;
	snr=1.0;
	snr_db=1.0;
	/*printf("SNR infinito !!!\n");
	  return 1;*/
      }
      
      printf("Energy_A\t=\t%f\n",energy_A);
      printf("Energy_B\t=\t%f\n",energy_B);
      printf("Energy_error\t=\t%f\n",energy_error);
      printf("Number of samples\t=\t%d\n",counter);
      printf("MSE\t=\t%f\n",mse);
      printf("RMSE\t=\t%f\n",rmse);
      printf("SNR\t=\t%f\n",snr);
      printf("SNR[dB]\t=\t%f\n",snr_db);
      printf("PSNR\t=\t%f\n",psnr);
      printf("PSNR[dB]\t=\t%f\n",psnr_db);

    } else {
      /*unsigned*/long long energy_A=0, energy_B=0, energy_error=0;
      for(block = 0;; block++) {
	long long local_energy_error = energy_error, local_energy_A = energy_A;
	int local_counter = counter;
	int r_A = fread(buf_A,sizeof(unsigned char),block_size,file_A);
	int r_B = fread(buf_B,sizeof(unsigned char),block_size,file_B);
	int min = r_B;
	if(r_A < r_B) min = r_A;
	if(min==0) break;
	{
	  int a,b,ab;
	  for(i=0; i<min; i++) {
	    a=buf_A[i];
	    b=buf_B[i];
	    energy_A += a*a;
	    energy_B += b*b;
	    ab = a - b;
	    energy_error += ab*ab;
	    counter++;
	  }
	}
	if(energy_error) {
	  local_energy_error = energy_error - local_energy_error;
	  local_counter = counter - local_counter;
	  local_energy_A = energy_A - local_energy_A;
	  mse=(double)(local_energy_error)/(double)(local_counter);
	  psnr=(double)(peak)*(double)(peak)/mse;
	  psnr_db=10.0*log10(psnr);
	  rmse=sqrt(mse);
	  snr=(double)(local_energy_A)/(double)(local_energy_error);
	  snr_db=10.0*log10(snr);
	} else {
	  snr_db=1.0;
	  psnr_db=1.0;
	  /*printf("SNR infinito !!!\n");
	    return 1;*/
	}
	fprintf(stderr,"%3d\t%f\n", block, psnr_db);
      }

      if(energy_error) {
	mse=(double)(energy_error)/(double)(counter);
	psnr=(double)(peak)*(double)(peak)/mse;
	psnr_db=10.0*log10(psnr);
	rmse=sqrt(mse);
	snr=(double)(energy_A)/(double)(energy_error);
	snr_db=10.0*log10(snr);
      } else {
	mse=0.0;
	psnr=1.0;
	psnr_db=1.0;
	rmse=0.0;
	snr=1.0;
	snr_db=1.0;
	/*printf("SNR infinito !!!\n");
	  return 1;*/
      }
      
      printf("Energy_A\t=\t%Ld\n",energy_A);
      printf("Energy_B\t=\t%Ld\n",energy_B);
      printf("Energy_error\t=\t%Ld\n",energy_error);
      printf("Number of samples\t=\t%d\n",counter);
      printf("MSE\t=\t%f\n",mse);
      printf("RMSE\t=\t%f\n",rmse);
      printf("SNR\t=\t%f\n",snr);
      printf("SNR[dB]\t=\t%f\n",snr_db);
      printf("PSNR\t=\t%f\n",psnr);
      printf("PSNR[dB]\t=\t%f\n",psnr_db);

    }
  }

  
  if (!strcmp(type,"ushort")) {
    unsigned short *buf_A = (unsigned short *)malloc(block_size*sizeof(unsigned short));
    unsigned short *buf_B = (unsigned short *)malloc(block_size*sizeof(unsigned short));
    int block;
    /*unsigned*/long long energy_A=0, energy_B=0, energy_error=0;
    printf("%s: data type: unsigned short\n",argv[0]);
    for(block = 0;; block++) {
      long long local_energy_error = energy_error, local_energy_A = energy_A;
      int local_counter = counter;
      int r_A = fread(buf_A,sizeof(unsigned short),block_size,file_A);
      int r_B = fread(buf_B,sizeof(unsigned short),block_size,file_B);
      int min = r_B;
      if(r_A < r_B) min = r_A;
      if(min==0) break;
      {
	int a,b,ab;
	for(i=0; i<min; i++) {
	  a=buf_A[i];
	  b=buf_B[i];
	  energy_A += a*a;
	  energy_B += b*b;
	  ab = a - b;
	  energy_error += ab*ab;
	  counter++;
	}
      }
      if(energy_error) {
	local_energy_error = energy_error - local_energy_error;
	local_counter = counter - local_counter;
	local_energy_A = energy_A - local_energy_A;
	mse=(double)(local_energy_error)/(double)(local_counter);
	psnr=(double)(peak)*(double)(peak)/mse;
	psnr_db=10.0*log10(psnr);
	rmse=sqrt(mse);
	snr=(double)(local_energy_A)/(double)(local_energy_error);
	snr_db=10.0*log10(snr);
      } else {
	snr_db=1.0;
	psnr_db=1.0;
	/*printf("SNR infinito !!!\n");
	  return 1;*/
      }
      fprintf(stderr,"%3d\t%f\n", block, psnr_db);
    }
      if(energy_error) {
	mse=(double)(energy_error)/(double)(counter);
	psnr=(double)(peak)*(double)(peak)/mse;
	psnr_db=10.0*log10(psnr);
	rmse=sqrt(mse);
	snr=(double)(energy_A)/(double)(energy_error);
	snr_db=10.0*log10(snr);
      } else {
	mse=0.0;
	psnr=1.0;
	psnr_db=1.0;
	rmse=0.0;
	snr=1.0;
	snr_db=1.0;
	/*printf("SNR infinito !!!\n");
	  return 1;*/
      }
      
      printf("Energy_A\t=\t%Ld\n",energy_A);
      printf("Energy_B\t=\t%Ld\n",energy_B);
      printf("Energy_error\t=\t%Ld\n",energy_error);
      printf("Number of samples\t=\t%d\n",counter);
      printf("MSE\t=\t%f\n",mse);
      printf("RMSE\t=\t%f\n",rmse);
      printf("SNR\t=\t%f\n",snr);
      printf("SNR[dB]\t=\t%f\n",snr_db);
      printf("PSNR\t=\t%f\n",psnr);
      printf("PSNR[dB]\t=\t%f\n",psnr_db);
  }
  
  //printf("%s: counter=%d\n",argv[0],counter);
  //printf("%s: energy_error=%Ld\n",argv[0],energy_error);
  
}
