#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <vector>
#include <windows.h>
#include <sstream>

#include "svm.h"
#include "Serial.h"

#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

using namespace std;
struct svm_parameter param;
struct svm_problem prob;
struct svm_model *model;
struct svm_node *x_space;
struct svm_node *x;

static char *line = NULL;
static int max_line_len;
string input_file_name;

CSerial serial;
int max_nr_attr = 64;
int predict_probability=0;
static int (*info)(const char *fmt,...) = &printf;

static char* readline(FILE *input){
	int len;

	if(fgets(line,max_line_len,input) == NULL)
		return NULL;

	while(strrchr(line,'\n') == NULL)
	{
		max_line_len *= 2;
		line = (char *) realloc(line,max_line_len);
		len = (int) strlen(line);
		if(fgets(line+len,max_line_len-len,input) == NULL)
			break;
	}
	return line;
}
void greetings(){
    cout << "Simple SVM C++ Version 1.0" << endl << "Initialized ! " << endl;
    cout << "Training Dataset Filename = " ;
    cin >> input_file_name;
}
void exit_input_error(int line_num){
	fprintf(stderr,"Wrong input format at line %d\n", line_num);
	exit(1);
}
void options(){
    // default values
	param.svm_type = C_SVC;
	param.kernel_type = LINEAR;
	param.degree = 3;
	param.gamma = 0;	// 1/num_features
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 100;
	param.C = 100; //DEFAULT 1
	param.eps = 1e-3;
	param.p = 0.1;
	param.shrinking = 1;
	param.probability = 0;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;

}

// read in a problem (in svmlight format)
void read_problem(string myfilename){
	int max_index, inst_max_index, i;
	size_t elements, j;

    const char *filename = myfilename.c_str();
	FILE *fp = fopen(filename,"r");
	char *endptr;
	char *idx, *val, *label;

	if(fp == NULL)
	{
		fprintf(stderr,"can't open input file %s\n",filename);
		exit(1);
	}

	prob.l = 0;
	elements = 0;

	max_line_len = 1024;
	line = Malloc(char,max_line_len);
	while(readline(fp)!=NULL)
	{
		char *p = strtok(line," \t"); // label

		// features
		while(1)
		{
			p = strtok(NULL," \t");
			if(p == NULL || *p == '\n') // check '\n' as ' ' may be after the last feature
				break;
			++elements;
		}
		++elements;
		++prob.l;
	}
	rewind(fp);

	prob.y = Malloc(double,prob.l);
	prob.x = Malloc(struct svm_node *,prob.l);
	x_space = Malloc(struct svm_node,elements);

	max_index = 0;
	j=0;
	for(i=0;i<prob.l;i++)
	{
		inst_max_index = -1; // strtol gives 0 if wrong format, and precomputed kernel has <index> start from 0
		readline(fp);
		prob.x[i] = &x_space[j];
		label = strtok(line," \t\n");
		if(label == NULL) // empty line
			exit_input_error(i+1);

		prob.y[i] = strtod(label,&endptr);
		if(endptr == label || *endptr != '\0')
			exit_input_error(i+1);

		while(1)
		{
			idx = strtok(NULL,":");
			val = strtok(NULL," \t");

			if(val == NULL)
				break;

			errno = 0;
			x_space[j].index = (int) strtol(idx,&endptr,10);
			if(endptr == idx || errno != 0 || *endptr != '\0' || x_space[j].index <= inst_max_index)
				exit_input_error(i+1);
			else
				inst_max_index = x_space[j].index;

			errno = 0;
			x_space[j].value = strtod(val,&endptr);
			if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
				exit_input_error(i+1);

			++j;
		}

		if(inst_max_index > max_index)
			max_index = inst_max_index;
		x_space[j++].index = -1;
	}

	if(param.gamma == 0 && max_index > 0)
		param.gamma = 1.0/max_index;

	if(param.kernel_type == PRECOMPUTED)
		for(i=0;i<prob.l;i++)
		{
			if (prob.x[i][0].index != 0)
			{
				fprintf(stderr,"Wrong input format: first column must be 0:sample_serial_number\n");
				exit(1);
			}
			if ((int)prob.x[i][0].value <= 0 || (int)prob.x[i][0].value > max_index)
			{
				fprintf(stderr,"Wrong input format: sample_serial_number out of range\n");
				exit(1);
			}
		}

	fclose(fp);
}
void predict(FILE *input, FILE *output){
	int correct = 0;
	int total = 0;
	double error = 0;
	double sump = 0, sumt = 0, sumpp = 0, sumtt = 0, sumpt = 0;

	int svm_type=svm_get_svm_type(model);
	int nr_class=svm_get_nr_class(model);
	double *prob_estimates=NULL;
	int j;

	if(predict_probability){
		if (svm_type==NU_SVR || svm_type==EPSILON_SVR)
			info("Prob. model for test data: target value = predicted value + z,\nz: Laplace distribution e^(-|z|/sigma)/(2sigma),sigma=%g\n",svm_get_svr_probability(model));
		else{
			int *labels=(int *) malloc(nr_class*sizeof(int));
			svm_get_labels(model,labels);
			prob_estimates = (double *) malloc(nr_class*sizeof(double));
			fprintf(output,"labels");
			for(j=0;j<nr_class;j++)
				fprintf(output," %d",labels[j]);
			fprintf(output,"\n");
			free(labels);
		}
	}

	max_line_len = 1024;
	line = (char *)malloc(max_line_len*sizeof(char));

	while(readline(input) != NULL){

		int i = 0;
		double target_label, predict_label;
		char *idx, *val, *label, *endptr;
		int inst_max_index = -1; // strtol gives 0 if wrong format, and precomputed kernel has <index> start from 0

		label = strtok(line," \t\n");
		if(label == NULL) // empty line
			exit_input_error(total+1);

		target_label = strtod(label,&endptr);
		if(endptr == label || *endptr != '\0')
			exit_input_error(total+1);

		while(1)
		{

			if(i>=max_nr_attr-1)	// need one more for index = -1
			{
				max_nr_attr *= 2;
				x = (struct svm_node *) realloc(x,max_nr_attr*sizeof(struct svm_node));
			}

			idx = strtok(NULL,":");
			val = strtok(NULL," \t");

			if(val == NULL)
				break;
			errno = 0;

			x[i].index = (int) strtol(idx,&endptr,10);
			if(endptr == idx || errno != 0 || *endptr != '\0' || x[i].index <= inst_max_index)
				exit_input_error(total+1);
			else
				inst_max_index = x[i].index;

			errno = 0;
			x[i].value = strtod(val,&endptr);
			if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
				exit_input_error(total+1);

			++i;
		}


		x[i].index = -1;

		if (predict_probability && (svm_type==C_SVC || svm_type==NU_SVC))
		{
			predict_label = svm_predict_probability(model,x,prob_estimates);
			fprintf(output,"%g",predict_label);
			for(j=0;j<nr_class;j++)
				fprintf(output," %g",prob_estimates[j]);
			fprintf(output,"\n");
		}
		else
		{
			predict_label = svm_predict(model,x);
			fprintf(output,"%g\n",predict_label);
		}

		if(predict_label == target_label)
			++correct;
		error += (predict_label-target_label)*(predict_label-target_label);
		sump += predict_label;
		sumt += target_label;
		sumpp += predict_label*predict_label;
		sumtt += target_label*target_label;
		sumpt += predict_label*target_label;
		++total;
	}

	if (svm_type==NU_SVR || svm_type==EPSILON_SVR)
	{
		info("Mean squared error = %g (regression)\n",error/total);
		info("Squared correlation coefficient = %g (regression)\n",
			((total*sumpt-sump*sumt)*(total*sumpt-sump*sumt))/
			((total*sumpp-sump*sump)*(total*sumtt-sumt*sumt))
			);
	}
	else
		info("Accuracy = %g%% (%d/%d) (classification)\n",
			(double)correct/total*100,correct,total);
	if(predict_probability)
		free(prob_estimates);
}
int numDigits(int number){
    int digits = 0;
    if (number < 0) digits = 1;
    while (number) {
        number /= 10;
        digits++;
    }
    return digits;
}
void serial_sent_int(int input){
    //Find The Amount Of Digits
        int digits = numDigits(input);
        if(input == 0) digits = 1;
    //Convert to CONST CHAR * For transfer
        stringstream temp_str;
        temp_str << (input);
        string str = temp_str.str();
        const char * tempChar = str.c_str();
    //Transfer via Serial
        cout << "[Serial] Sending : " << tempChar << endl;
        serial.SendData(tempChar,digits);
    //Ending Seperator
        serial.SendData(",",1);
        Sleep(100);
}
string convertDouble(double value) {
  std::ostringstream o;
  if (!(o << value))
    return "";
  return o.str();
}


void serial_sent_double(double input){
    //Find the Amount of Integer
    int int_digits = numDigits((int)input); //Cast to int then find
    //convert to CONST CHAR * with fixing 6 precision of decimal
    char tempChar[50];
    int digits = int_digits + 1 + 6;
    snprintf(tempChar,50,"%f",input);
    //Transfer via Serial
        cout << "[Serial] Sending : " << tempChar << endl;
        serial.SendData(tempChar,digits);
    //Ending Seperator
        serial.SendData(",",1);

        Sleep(100);

}

int main(){
    //Show Welcome Message and Get DATASET Filename
    greetings();
    //Set the options
    options();
    //Read the Dataset
    read_problem(input_file_name);
    //Check If Error Occurred when set the options and filename
    const char *error_msg;
    error_msg = svm_check_parameter(&prob,&param);
	if(error_msg){
		cout << " Error Detected ! " << endl << "(error)  = " <<error_msg << endl;
		exit(1);
	}

    //Begin The Training Process
	model = svm_train(&prob,&param);

//    Use The Model as an Predictor
    int correct = 0;
	int total = 0;
	double error = 0;
	double sump = 0, sumt = 0, sumpp = 0, sumtt = 0, sumpt = 0;
    int svm_type=svm_get_svm_type(model);
	int nr_class=svm_get_nr_class(model);
	double *prob_estimates=NULL;
	int j;
   // ====================================
   // SENT TO SERIAL
     cout << "[Serial] Opening Serial Port Comm. . . . " <<endl;
    if (serial.Open(4, 9600)){
     cout << "[Serial] Port opened successfully" << endl;
    }else{
     cout << "[Serial] Failed to open port!" << endl;
    }
    serial_sent_int(model->nr_class);

    serial_sent_int(model->l);

            int rhosize = model->nr_class*(model->nr_class-1)/2;
            for(int i = 0 ; i <rhosize ; i++){
                serial_sent_double(model->rho[i]);

            }

            for(int i = 0 ; i < model->nr_class ; i++){
                serial_sent_int(model->label[i]);

            }

            for(int i = 0 ; i < model->nr_class ; i++){
                serial_sent_int(model->nSV[i]);

            }
    serial.Close();
    return 0;
}



//PREDICTION MODULE on DESKTOP

  /*
    string model_file_name;
    cout << "Model File Name = ? > " ;
	cin >> model_file_name;
	const char *model_file = model_file_name.c_str();
    svm_save_model(model_file, model);

    cout << ""
    //====================================
    string test_file_name,output_file_name;
	cout << "Test File Name = ? > " ;
	cin >> test_file_name;

	cout << "Output Result File Name = ? ( Create your Own one ) > " ;
	cin >> output_file_name;

    const char *test_file = test_file_name.c_str();
	FILE *input_test = fopen(test_file,"r");
	const char *out_file = output_file_name.c_str();
	FILE *output = fopen(out_file,"w");
    //====================================
    x = (struct svm_node *) malloc(max_nr_attr*sizeof(struct svm_node));
    predict(input_test,output);
    */
