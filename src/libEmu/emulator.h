#ifndef __EMULATOR_INC_
#define __EMULATOR_INC_
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h> 
#include <gsl/gsl_sf.h>


//! a structure which controls which options to use in the actual emualtor process
typedef struct emulator_opts{
	//! the smoothness of the "gaussian" cov fn, 2.0 is infinitely diff, 1.0 is super jaggy
	double alpha;
	//! set this to 1 to use the full (variable nu) matern cov fn
	int usematern; 
	//! set this to 1 to use a 3/2 (fixed nu) matern cov fn
	int usematern_three;
	//! set this to 1 to use a 5/2 (fixed nu) matern cov fn
	int usematern_five;
} emulator_opts;


void print_matrix(gsl_matrix* m, int nx, int ny);
void print_emulator_options(emulator_opts* x);
void set_emulator_defaults(emulator_opts* x);
double covariance_fn(gsl_vector *xm, gsl_vector* xn, gsl_vector* thetas, int nthetas, int nparams);
double covariance_fn_gaussian(gsl_vector *xm, gsl_vector* xn, gsl_vector* thetas, int nthetas, int nparams, double alpha);
double covariance_fn_matern(gsl_vector *xm, gsl_vector* xn, gsl_vector* thetas, int nthetas, int nparams);
double covariance_fn_matern_three(gsl_vector *xm, gsl_vector* xn, gsl_vector* thetas, int nthetas, int nparams);
double covariance_fn_matern_five(gsl_vector *xm, gsl_vector* xn, gsl_vector* thetas, int nthetas, int nparams);
void makeKVector(gsl_vector* kvector, gsl_matrix *xmodel, gsl_vector *xnew, gsl_vector* thetas, int nmodel_points, int nthetas, int nparams);
void makeCovMatrix(gsl_matrix* cov_matrix, gsl_matrix *xmodel, gsl_vector* thetas, int nmodel_points, int nthetas, int nparams);
double makeEmulatedMean(gsl_matrix *inverse_cov_matrix, gsl_vector *training_vector, gsl_vector *kplus_vector, int nmodel_points);
double makeEmulatedVariance(gsl_matrix *inverse_cov_matrix, gsl_vector *kplus_vector, double kappa, int nmodel_points);
void initialise_new_x(gsl_matrix* new_x, int nparams, int nemulate_points, double emulate_min, double emulate_max);
#endif
