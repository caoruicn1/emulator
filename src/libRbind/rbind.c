#include "rbind.h"

void callEmulator(double* xmodel_in, int *nparams_in, double* training_in, int *nmodelpts, int *nthetas, double* final_x, int* nemupts, \
									double* finaly, double* finalvar, double* rangemin, double* rangemax){
	double *thetas_vec;
	thetas_vec = MallocChecked(sizeof(double)*(*nthetas));
	// this will run the estimator and fill in the thetas vector with the best ones
	callEstimate(xmodel_in, nparams_in, training_in, nmodelpts, nthetas, thetas_vec);
	callEmulate(xmodel_in, nparams_in, training_in, nmodelpts, thetas_vec, nthetas, final_x, nemupts, finaly, finalvar, rangemin, rangemax);
	free(thetas_vec);
}


/**
 * calculate the thetas for a model
 * 
 * post-refactoring this is wonderfully simple
 */
void callEstimate(double* xmodel_in, int* nparams_in, double* training_in, int *nmodelpts, int *nthetas_in, double* final_thetas){
	int i;
	optstruct options;
	modelstruct the_model;

	
	// setup the optstruct
	options.nmodel_points = *nmodelpts;
	options.nparams = *nparams_in;
	options.nthetas = *nthetas_in;
	options.emulate_min = EMULATEMINDEFAULT;
	options.emulate_max = EMULATEMAXDEFAULT;
	options.grad_ranges = gsl_matrix_alloc(options.nthetas, 2);
	options.nregression_fns = 1 + options.nparams;		// simple constant regression
	setup_cov_fn(&options);
	setup_optimization_ranges(&options);

	alloc_modelstruct(&the_model, &options);

	// fill in xmodel, this is the right way!
	// note that xmodel has to be transposd in EmuRbind.R or else disaster
	printf("nx = %d, ny = %d\n", options.nparams, options.nmodel_points);
	convertDoubleToMatrix(the_model.xmodel, xmodel_in, options.nparams, options.nmodel_points);
	// fill in the training vec
	convertDoubleToVector(the_model.training_vector, training_in, options.nmodel_points);

	// actually do the estimation using libEmu
	estimate_thetas_threaded(&the_model, &options);

	// set the final results
	for(i = 0; i < options.nthetas; i++)
		final_thetas[i] = gsl_vector_get(the_model.thetas, i);

	// tidy up
	free_modelstruct(&the_model);
	free_optstruct(&options);
}

/**
 * compute the mean and variance at a set of points
 * @return final_emulated_y is a vector of the mean at each point
 * @return final_emulated_variance is a vector of the variance at each point
 * @param nemupoints is a list of how many points to emulate
 * @param points_in is a double array of nparams x nemupoints 
 */
void callEmulateAtList(double *xmodel_in, int *nparams_in, double* points_in, int *nemupoints, double* training_in,
											 int *nmodelpts, double* thetas_in, int *nthetas_in, double* final_emulated_y, 
											 double* final_emulated_variance){
	optstruct options;
	modelstruct the_model; 
	//double the_mean, the_variance;
	gsl_matrix *the_point_array;
	int i, j;

	options.nmodel_points = *nmodelpts;
	options.nparams = *nparams_in;
	options.nemulate_points = *nemupoints;
	// eep
	options.nregression_fns = 1 + options.nparams;
	options.emulate_min = 0; // again unused
	options.emulate_max = 1; 
	setup_cov_fn(&options);
	setup_optimization_ranges(&options); // not used

	fprintf(stderr, "callEmulate at list, nparams %d, nemulate_points %d\n", *nparams_in, *nemupoints);

	alloc_modelstruct(&the_model, &options);

	//the_point_array = gsl_matrix_alloc(options.nparams, options.nemulate_points);
	the_point_array = gsl_matrix_alloc(options.nemulate_points, options.nparams);


	// fill in the point array
	convertDoubleToMatrix(the_point_array, points_in, options.nparams, options.nemulate_points);
	//convertDoubleToMatrix(the_point_array, points_in, options.nemulate_points, options.nparams);
	

	/* for(i = 0; i < *nemupoints; i++){ */
	/* 	for(j = 0; j < *nparams_in; j++){ */
	/* 		fprintf(stderr, "%lf ", gsl_matrix_get(the_point_array, i, j)); */
	/* 	} */
	/* 	fprintf(stderr,"\n"); */
	/* } */
							


	// fill in thetas
	convertDoubleToVector(the_model.thetas, thetas_in, options.nthetas);
	// fill in xmodel 
	convertDoubleToMatrix(the_model.xmodel, xmodel_in, options.nparams, options.nmodel_points);
	// fill in the training vec
	convertDoubleToVector(the_model.training_vector, training_in, options.nmodel_points);

	
	// now do emulate at point list
	// need to implement this ... 
	emulateAtPointList(&the_model, the_point_array, &options, final_emulated_y, final_emulated_variance);	

	
	gsl_matrix_free(the_point_array);
	// tidy up
	free_modelstruct(&the_model);
	free_optstruct(&options);

}

/**
 * computes the mean and variance at point_in 
 * @return final_emulated_y is the mean at point
 * @return final_emulated_variance is the variance at point
 * @param point_in is a double array of nparams length, the point that we wish to eval the emulator at 
 */
void callEmulateAtPt(double* xmodel_in, int* nparams_in, double* point_in, double* training_in, int* nmodelpts, double* thetas_in, int* nthetas_in, double* final_emulated_y, double* final_emulated_variance){
	optstruct options;
	modelstruct the_model;
	double the_mean, the_variance;
	gsl_vector *the_point;
	int i;
	
	options.nmodel_points = *nmodelpts;
	options.nparams = *nparams_in;
	options.nemulate_points = 1;
	options.nthetas = *nthetas_in;
	// fuck this needs to be set automatically :(
	options.nregression_fns = 1 + options.nparams;
	options.emulate_min = 0; // won't be used
	options.emulate_max = 1;
	setup_cov_fn(&options);
	setup_optimization_ranges(&options);	// this strictly isn't needed for emulator

	// noisy
	/* fprintf(stderr, "nparams:%d\tnmodel_pts:%d\tnthetas:%d\n", options.nparams, options.nmodel_points, options.nthetas); */
	/* fprintf(stderr,"point:"); */
	/* for(i = 0; i < options.nparams; i++)	  */
	/* 	fprintf(stderr,"\t%lf", point_in[i]); */
	/* fprintf(stderr,"\n"); */

	alloc_modelstruct(&the_model, &options);

	
	the_point = gsl_vector_alloc(options.nparams);
	
	/* checking convertDoubleToVector
	 * 	fprintf(stderr, "point pre-alloc: ");
	 * print_vector_quiet(the_point, options.nparams);
	 */

	convertDoubleToVector(the_point, point_in, options.nparams);

	/* check tht convertDoubleToVector works 
	 * fprintf(stderr, "point post-alloc: ");
	 * print_vector_quiet(the_point, options.nparams);
	 */
	
	// fill in thetas
	convertDoubleToVector(the_model.thetas, thetas_in, options.nthetas);
	// fill in xmodel 
	convertDoubleToMatrix(the_model.xmodel, xmodel_in, options.nparams, options.nmodel_points);
	// fill in the training vec
	convertDoubleToVector(the_model.training_vector, training_in, options.nmodel_points);
	
	emulateAtPoint(&the_model, the_point, &options, &the_mean, &the_variance);

	//print_vector_quiet(the_point,options.nparams);
	fprintf(stderr, "#mean:%lf\tvar:%lf\n", the_mean, the_variance);
	*final_emulated_y  = the_mean;
	*final_emulated_variance = the_variance;

	gsl_vector_free(the_point);
	// tidy up
	free_modelstruct(&the_model);
	free_optstruct(&options);
}

/**
 * run the emulator against a given set of data and given hyperparams theta
 */
void callEmulate(double* xmodel_in, int* nparams_in, double* training_in, int* nmodelpts, double* thetas_in, int* nthetas_in, double* final_emulated_x, int *nemupts_in, double* final_emulated_y, double* final_emulated_variance, double* range_min_in, double*range_max_in){
	
	optstruct options;
	modelstruct the_model;
	resultstruct results;

	int i, j;
	options.nmodel_points = *nmodelpts;
	options.nparams = *nparams_in;
	options.nemulate_points = *nemupts_in;
	options.nthetas = *nthetas_in;
	// fuck this needs to be set automatically :(
	options.nregression_fns = 1 + options.nparams;
	options.emulate_min = *range_min_in;
	options.emulate_max = *range_max_in;
	setup_cov_fn(&options);
	setup_optimization_ranges(&options);	// this strictly isn't needed for emulator

	alloc_modelstruct(&the_model, &options);

	alloc_resultstruct(&results, &options);
	
	// fill in thetas
	convertDoubleToVector(the_model.thetas, thetas_in, options.nthetas);
	// fill in xmodel 
	convertDoubleToMatrix(the_model.xmodel, xmodel_in, options.nparams, options.nmodel_points);
	// fill in the training vec
	convertDoubleToVector(the_model.training_vector, training_in, options.nmodel_points);

	/* for(i = 0; i < options.nmodel_points; i++){ */
	/* 	printf("%lf\n", gsl_vector_get(the_model.training_vector, i)); */
	/* } */

	/* printf("nparams:%d\n", options.nparams); */
	/* for(i = 0; i < options.nparams; i++){ */
	/* 	for(j = 0; j < options.nmodel_points; j++){ */
	/* 		printf("%lf\t", gsl_matrix_get(the_model.xmodel, j, i)); */
	/* 	} */
	/* 	printf("\n"); */
	/* } */
						 

	// and call out to libEmu
	emulate_model_results(&the_model, &options, &results);
	
	// Fill in emulated_y, emulated_variance
	for(i = 0; i < options.nemulate_points; i++){
		final_emulated_y[i] = gsl_vector_get(results.emulated_mean, i);
		final_emulated_variance[i] = gsl_vector_get(results.emulated_var, i);
	}

	// fill in final emulated_x
	for(j = 0; j < options.nparams; j++){
		for(i = 0; i < options.nemulate_points; i++){
			final_emulated_x[i+j*options.nmodel_points] = gsl_matrix_get(results.new_x, i, j);
		}
	}

	printf("hello from rbind-emulator\n");

	for(i = 0; i < options.nthetas; i++)
		printf("%g\t", gsl_vector_get(the_model.thetas, i));
	printf("\n");
					 
	printf("nemulate pts = %d\n", options.nemulate_points);

	// check that the results struct agrees with final_emulated etc
	for(i = 0; i < options.nemulate_points; i++){
		for(j = 0; j < options.nparams; j++){
			printf("%d:%g\t%g\t", i+j*options.nemulate_points, gsl_matrix_get(results.new_x, i, j), final_emulated_x[i+j*options.nemulate_points]);
		}
		
		printf("%g\t%g\t", gsl_vector_get(results.emulated_mean, i), final_emulated_y[i]);
		printf("%g\t%g\n", gsl_vector_get(results.emulated_var, i), final_emulated_variance[i]);
	}
	

	// tidy up
	free_modelstruct(&the_model);
	free_optstruct(&options);
	free_resultstruct(&results);
}



/**
 * this is a binding to call the function libEmu/maximise.c:evalLikelyhood
 * double evalLikelyhood(gsl_vector *vertex, gsl_matrix *xmodel, gsl_vector *trainingvector, 
 * int nmodel_points, int nthetas, int nparams) 
 * 
 * this is mostly copied from above
 */

void callEvalLikelyhood(double * xmodel_in, int* nparams_in, double* training_in, \
													int *nmodelpts_in, int* nthetas_in, double* thetas_in, \
													double* answer){
	optstruct options;
	modelstruct the_model;
	double the_likelyhood = 0.0;
	struct estimate_thetas_params params;
	const gsl_rng_type *T;
	
	T = gsl_rng_default;

	params.the_model = MallocChecked(sizeof(modelstruct));
	params.options = MallocChecked(sizeof(optstruct));

	
	options.nmodel_points = *nmodelpts_in;
	options.nparams = *nparams_in;
	options.nthetas = *nthetas_in;
	options.nregression_fns = options.nparams+1;
	setup_cov_fn(&options);
	setup_optimization_ranges(&options);
	
	alloc_modelstruct(&the_model, &options);
	convertDoubleToMatrix(the_model.xmodel, xmodel_in, options.nparams, options.nmodel_points);
	convertDoubleToVector(the_model.training_vector, training_in, options.nmodel_points);
	convertDoubleToVector(the_model.thetas, thetas_in, options.nthetas);

	// copy in the structures we just created
	copy_optstruct(params.options, &options);
	alloc_modelstruct(params.the_model, &options);
	copy_modelstruct(params.the_model, &the_model);
	
	params.random_number = gsl_rng_alloc(T);
	gsl_rng_set(params.random_number, get_seed_noblock());

	// this calls the log likelyhood
	the_likelyhood = evalLikelyhoodLBFGS_struct(&params);

	*answer = the_likelyhood;
	
	// tidy up
	gsl_rng_free(params.random_number);
	free_modelstruct(params.the_model);
	free_modelstruct(&the_model);
	gsl_matrix_free(options.grad_ranges);
	gsl_matrix_free(params.options->grad_ranges);
	gsl_matrix_free(params.h_matrix);
	free(params.the_model);
	free(params.options);
	 
	

}

//! creates the coeffs for a lagrange poly interpolation
/**
 *  @param xin are the points at which we require our polynomial to pass through
 *  @param valin are the values of the function we wish to interpolate at xin
 *  @param npts how many of xin there are, this also determines the order of the L poly
 *  @param where we want this to be evaluated
 *  @return desired_point_in is set to the correct value
 *
 * this is clearly possible in R but i just can't quite get it right
 */
void lagrange_interp(double* xin, double* valin, int* npts_in, double* desired_point_in){
	int i, j;
	double retVal = 0;
	double weight = 0.0;
	double npts = *npts_in;
	double desired_point = *desired_point_in;
	
	for(i=0; i<npts; ++i){
		weight = 1.0;
		for(j=0; j < npts; ++j){
			if( j != i){
				weight *= (desired_point - xin[j]) / (xin[i] - xin[j]);
			}
		}
		retVal += weight*valin[i];
	}
	// push the answer back into the desired_point_in
	*desired_point_in = retVal;
	
}

void testConvert(double* matrix, int *nx_in, int *ny_in){
	int nx = *nx_in;
	int ny = *ny_in;
	printf("nx = %d\tny= %d\n", nx, ny);
	gsl_matrix *test = gsl_matrix_alloc(ny, nx);
	convertDoubleToMatrix(test, matrix, nx, ny);
	gsl_matrix_free(test);
}



/*
 * takes an ALLOCATED gsl_matrix and copies the input vector into it, 
 * doesn't check anything
 * 
 * matrices in R are stored like this
 *      [,1] [,2] [,3] [,4]
 * [1,]    1    4    7    0
 * [2,]    2    5    8    0
 * [3,]    3    6    9    0
 * 
 * the input vector will look like this:
 * nx = 4	ny= 3
 * IN:0 = 1.000000
 * IN:1 = 2.000000
 * IN:2 = 3.000000
 * IN:3 = 4.000000
 * IN:4 = 5.000000
 * IN:5 = 6.000000
 * IN:6 = 7.000000
 * IN:7 = 8.000000
 * IN:8 = 9.000000
 * IN:9 = 0.000000
 * IN:10 = 0.000000
 * IN:11 = 0.000000
 *
 * as far as c is concerned matrices are going to be indexed by y and then x, i.e
 * a faithful copy of the input matrix should have:
 * 
 * m[1,1] = 1, m[2,1] = , m[3,2] =6 etc
 * does this work?
 */
void convertDoubleToMatrix(gsl_matrix* the_matrix, double* input, int nx, int ny){
	int i, j;
	/* for(i = 0; i < nx*ny; i++){ */
	/* 	printf("IN:%d = %lf\n", i, input[i]); */
	/* } */
	/* printf("\n"); */
	/* printf("nx = %d\tny=%d\n", nx, ny); */
	for(i = 0; i < nx; i++){
		for(j =0; j < ny; j++){
			gsl_matrix_set(the_matrix, j, i, input[j+ny*i]);
			/* printf("j+ny*i = %d\n", j+ny*i); */
			/* printf("MAT(%d,%d)=%lf\n", j+1, i+1, gsl_matrix_get(the_matrix,j,i)); */
		}
	}

}

										 
/*
 * takes an ALLOCATED gsl_vector and copies the input vector into it
 * non checking again
 */
void convertDoubleToVector(gsl_vector* the_vec, double* input, int nx){
	int i;
	for(i =0; i < nx; i++){
		gsl_vector_set(the_vec, i, input[i]);
	}
}
