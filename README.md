# simpleSVM-Cplusplus

Internship Project 1 : TU-Chemnitz, Germany 

By Theppasith Nisitsukcharoen ( Chulalongkorn University, Thailand )

simpleSVM for Material Sensing studying

A Very Simple SVM Module which used " LIBSVM ".
The purpose of this program is to shorten the SVM code as much as possible
for the implementation prototype on FPGA or Microcontroller.

### Requirement 
** Desktop Side ** 

1. Windows 7 (According to serial transmission Library)
2. CODE::BLOCK (IDE for C++/C)  - Project files formatted for code:Block
  - [CODE:BLOCK](http://www.codeblocks.org/downloads)

**Microcontroller Side**

1. Source-Code here [SimpleSOM-MSP430](https://github.com/Tutorgaming/SimpleSOM-MSP430) 
2. MSP430F5529 Board (Texas instrument)
3. Code Composer Studio (Texas instument modded version of Eclipse) 

### Setting it up

* First, you have to clone this repo into your computer.
* You can use SOURCETREE or any git clients you want to.
```sh 
   $ cd path/to/your/workspace
   $ git clone https://github.com/Tutorgaming/simpleSVM-Cplusplus.git
```
* Then Open the project file using Code::Block [simpleSVM_c++.cbp]
* Here you go :D 
* 

### Workflow

* For the inputs we will read from the example input files which provide in this package
  - input1
  - input4c
* Program will ask you to insert the value of classes and dimensions
* There are some parameter which need to be set before compiling and running 

```
void options(){
    // SETTINGs for C-SVM LINEAR with COST 100
    // Param in command line = -t 0 -s 0 -c 100
	  param.svm_type = C_SVC;
	  param.kernel_type = LINEAR;
	  param.degree = 3; // Default Value
	  param.C = 100; // DEFAULT is 1
}
```


1.  Program will ask for the dataset filename (with extension) 
2.  You can choose between 2 modes by setting the flag "DESKTOP"
  -  Simulation on Computer ( both training and classification are done on desktop - Save as files ) 
  -  Sent the trained models to microcontroller and do the classification on the microcontroller (Led blinking as result)
3.  After Selecting the modes, Program will do the training according to the parameters set 
4.  Then, Send every model parameter to microcontroller 
5.  For the testing Part, input are received from desktop side , send to microcontroller and the result class are shown as Blinking LED 
