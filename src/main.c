#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "estimator.h"
#include "emulator.h"
#include "maximise.h"
#include "sys/time.h"


/* 
 * 1 -> read command line parameters
 * 2 -> read and process stream data
 * 3 -> estimate thetas
 * 4 -> calculate new means and variances
 * 5 -> output
 */

#define NTHETASDEFAULT 4
#define NPARAMSDEFAULT 1
#define NEMULATEDEFAULT 20
#define EMULATEMINDEFAULT 0.0
#define EMULATEMAXDEFAULT 4.0

//! holds command line options
/** 
 * designed to hold basic command line 
 * options
 */
typedef struct optstruct{
	int nthetas;
	int nparams;
	int nmodel_points;
	int nemulate_points;
	double emulate_min;
	double emulate_max;
	char  filename[128];
} optstruct;


void print_usage(void);
void parse_arguments(int argc, char** argv, optstruct* options);
void emulate_model(gsl_matrix* xmodel, gsl_vector* training, gsl_vector*thetas, optstruct* options);
void estimate_thetas(gsl_matrix* xmodel_input, gsl_vector* training_vector, gsl_vector* thetas, optstruct* options);
void read_input_bounded(gsl_matrix* model, gsl_vector* training, optstruct * options);
unsigned long int get_seed(void);
void read_input_fromfile(gsl_matrix *xmodel, gsl_vector *training, optstruct *options);

//! print the short-option switches
void print_usage(void){
	printf("emulator\n");
	printf("options are: \n");
	printf("t->number of thetas should be (3+nparams)\n");
	printf("p->number of params\n");
	printf("n->number of model_points\n");
	printf("m->number of emulator poits\n");
	printf("a->min emulator value\n");
	printf("b->max emulator value\n");
}
	

//! parse the command line 
void parse_arguments(int argc, char** argv, optstruct* options){
	int theta_val = NTHETASDEFAULT;
	int param_val = NPARAMSDEFAULT;
	int nemulate_val = NEMULATEDEFAULT;
	double min_val = EMULATEMINDEFAULT;
	double max_val = EMULATEMAXDEFAULT;
	char file[128];
	int nmodel_points = 0;
	int c;

	// default
	sprintf(file, "input.txt");

	// short options only
	while (( c = getopt(argc, argv, "f:t:p:n:m:a:b:?")) != -1)
		switch(c)
			{
			case '?':
				print_usage();
				exit(1);
			case 'f':
				sprintf(file, "%s", optarg);
				break;
			case 't':
				theta_val = atoi(optarg);
				break;
			case 'p':
				param_val = atoi(optarg);
				break;
			case 'n':
				nmodel_points = atoi(optarg);
				break;
			case 'm':
				nemulate_val = atoi(optarg);
				break;
			case 'a':
				min_val = strtod(optarg, NULL);
				break;
			case 'b':
				max_val = strtod(optarg, NULL);
				break;								 
			default:
				abort();
			}

	//\todo something is wrong with the theta_val thing
	options->nthetas = theta_val;
	options->nparams = param_val;
	options->nmodel_points = nmodel_points;
	options->nemulate_points = nemulate_val;
	options->emulate_min = min_val;
	options->emulate_max = max_val;
	sprintf(options->filename, "%s", file);
}
 


int main (int argc, char ** argv){
	optstruct options;
	parse_arguments(argc, argv, &options);	
	gsl_matrix* xmodel_input;
	gsl_vector* training_vector;
	gsl_vector* thetas;
	
	if(options.nmodel_points != 0){
		assert(options.nmodel_points >0);
		assert(options.nthetas >0);
		xmodel_input = gsl_matrix_alloc(options.nmodel_points, options.nmodel_points);
		training_vector = gsl_vector_alloc(options.nmodel_points);
		thetas = gsl_vector_alloc(options.nthetas);
		//read_input_fromfile(xmodel_input, training_vector, &options);
		read_input_bounded(xmodel_input, training_vector, &options); //(i'm too stupid for this)
	} else {
		//\todo write this
		//read_input_unbounded(xmodel_input, training_vector, options);
		exit(1);
	}

	estimate_thetas(xmodel_input, training_vector, thetas, &options);

	// calc the new means, new variance and dump to stdout
	emulate_model(xmodel_input, training_vector, thetas, &options);

	gsl_vector_free(thetas);
	gsl_vector_free(training_vector);
	gsl_matrix_free(xmodel_input);
	return(0);
}


void read_input_fromfile(gsl_matrix *xmodel, gsl_vector *training, optstruct *options){
	int i = 0; 
	int j = 0;
	double temp_value = 0.0;
	FILE *fptr;
	fptr = fopen(options->filename, "r");

	for(i =0; i < options->nmodel_points; i++){
		for(j = 0; j < options->nparams; j++){
			fscanf(fptr, "%lg", &temp_value);
			gsl_matrix_set(xmodel, i, j, temp_value);
		}
		fscanf(fptr, "%lg", &temp_value);
		gsl_vector_set(training, i, temp_value);
	}
	print_matrix(xmodel, options->nmodel_points, options->nparams);
	vector_print(training, options->nmodel_points);
}


void emulate_model(gsl_matrix* xmodel, gsl_vector* training, gsl_vector*thetas, optstruct* options){
	int i = 0;
	int j = 0; 
	int n_emu_points = options->nemulate_points;
	double step_size = (options->emulate_max - options->emulate_min) / ((double)n_emu_points);
	double temp_mean, temp_var;
	double kappa = 0; // for now
	gsl_matrix *new_x = gsl_matrix_alloc(n_emu_points, options->nparams);
	gsl_vector *new_mean = gsl_vector_alloc(n_emu_points);
	gsl_vector *new_variance = gsl_vector_alloc(n_emu_points);
	gsl_matrix *c_matrix = gsl_matrix_alloc(options->nmodel_points, options->nmodel_points);
	gsl_matrix *cinverse = gsl_matrix_alloc(options->nmodel_points, options->nmodel_points);
	gsl_vector *kplus = gsl_vector_alloc(options->nmodel_points);
	gsl_vector_view new_x_row;

	gsl_matrix *temp_matrix = gsl_matrix_alloc(options->nmodel_points, options->nmodel_points);
	gsl_permutation *c_LU_permutation = gsl_permutation_alloc(options->nmodel_points);
	int lu_signum = 0;

	makeCovMatrix(c_matrix, xmodel, thetas,options->nmodel_points, options->nthetas, options->nparams);
	gsl_matrix_memcpy(temp_matrix, c_matrix);
	gsl_linalg_LU_decomp(temp_matrix, c_LU_permutation, &lu_signum);
	gsl_linalg_LU_invert(temp_matrix, c_LU_permutation, cinverse);
	
	// set the new_x values
	for(i = 0; i < n_emu_points; i++){
		// this doesn't make sense for many params!
		for(j = 0; j < options->nparams; j++){
			gsl_matrix_set(new_x, i, j,step_size*((double)i)+options->emulate_min);
		}
	}


	for(i = 0; i < n_emu_points; i++){
		new_x_row = gsl_matrix_row(new_x, i);
		makeKVector(kplus, xmodel, &new_x_row.vector, thetas, options->nmodel_points, options->nthetas, options->nparams);
		temp_mean = makeEmulatedMean(cinverse, training, kplus, options->nmodel_points);
		kappa = covariance_fn(&new_x_row.vector, &new_x_row.vector, thetas, options->nthetas, options->nparams);
		temp_var = makeEmulatedVariance(cinverse, kplus, kappa, options->nmodel_points);
		gsl_vector_set(new_mean, i, temp_mean);
		gsl_vector_set(new_variance, i, temp_var);
	}
	 
	for(i = 0; i < n_emu_points; i++){
		for(j = 0; j < options->nparams; j++){
			printf("%g\t", gsl_matrix_get(new_x, i, j));
		}
		printf("%g\t", gsl_vector_get(new_mean, i));
		printf("%g\n", gsl_vector_get(new_variance, i));
	}
		
	gsl_matrix_free(new_x);
	gsl_vector_free(new_mean);
	gsl_vector_free(new_variance);
	gsl_matrix_free(c_matrix);
	gsl_matrix_free(cinverse);
	gsl_vector_free(kplus);
	gsl_matrix_free(temp_matrix);
	gsl_permutation_free(c_LU_permutation);
}




void estimate_thetas(gsl_matrix* xmodel_input, gsl_vector* training_vector, gsl_vector* thetas, optstruct* options){
	const gsl_rng_type *T;
	gsl_rng *random_number;
	int max_tries = 20;
	int i; 
	int number_steps = 40;
	gsl_matrix *grad_ranges = gsl_matrix_alloc(options->nthetas, 2);

	T = gsl_rng_default;
	random_number = gsl_rng_alloc(T);
	gsl_rng_set(random_number, get_seed());
	
	/* set the ranges for the initial values of the NM lookup, 
	 * might want to adjust these as required etc, but whatever */
	for(i = 0; i < options->nthetas; i++){
		gsl_matrix_set(grad_ranges, i, 0, 0.0);
		gsl_matrix_set(grad_ranges, i, 1, 1.0);
	}
	
	gsl_matrix_set(grad_ranges, 3, 0, 0.0);
	gsl_matrix_set(grad_ranges, 3, 0, 0.4);
	


	nelderMead(random_number, max_tries, number_steps, thetas, grad_ranges, xmodel_input, training_vector, options->nmodel_points, options->nthetas, options->nparams);

	fprintf(stderr, "best_thetas: \t");
	print_vector_quiet(thetas, options->nthetas);

	gsl_rng_free(random_number);
	gsl_matrix_free(grad_ranges);
}




void read_input_bounded(gsl_matrix* model, gsl_vector* training, optstruct * options){
	int i = 0;
	int j = 0;
	double temp_value;	 
	while(i < options->nmodel_points){
		for(j = 0; j < options->nparams; j++){
			scanf("%lg", &temp_value);
			//printf("%lg\n", temp_value);
			gsl_matrix_set(model, i, j, temp_value);
		}
		scanf("%lg", &temp_value);
		//printf("%lg\n", temp_value);
		gsl_vector_set(training, i, temp_value);
		i++;
	};

	//printf("read in xmodel:\n");
	//print_matrix(model, options->nmodel_points, options->nparams);
	//printf("read in training_vec:\n");
	//vector_print(training, options->nmodel_points);


}




// RNG 
// tries to read from /dev/random, or otherwise uses the system time
unsigned long int get_seed(void){
	unsigned int seed;
	struct timeval tv;
	FILE *devrandom;

	if((devrandom = fopen("/dev/random", "r")) == NULL){
		gettimeofday(&tv, 0);
		seed = tv.tv_sec + tv.tv_usec;
		fprintf(stderr,"Got seed %u from gettimeofday()\n", seed);
	}
	else {
		fread(&seed, sizeof(seed), 1, devrandom);
		fprintf(stderr,"Got seed %u from /dev/random\n", seed);
		fclose(devrandom);
	}
	return(seed);
}