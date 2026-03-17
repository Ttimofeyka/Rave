# std/math

Mathematical functions including trigonometry, exponentials, and neural network activation functions.

## Constants

- `std::math::PI` - Pi constant (double precision)
- `std::math::PI_f` - Pi constant (float precision)
- `std::math::PI_hf` - Pi constant (half precision)
- `std::math::NAN` - Not-a-Number (double)
- `std::math::NAN_f` - Not-a-Number (float)
- `std::math::NAN_hf` - Not-a-Number (half)

## Basic Functions

### abs
```d
T abs<T>(T value)
```
Returns absolute value of a number.

### sign
```d
char sign<T>(T x)
```
Returns -1 for negative, 0 for zero, 1 for positive.

### getSign
```d
char getSign<T>(T value)
```
Returns 1 for positive/zero, -1 for negative.

### copySign
```d
T copySign<T>(T v1, T v2)
```
Returns v1 with the sign of v2.

### min
```d
T min<T>(T one, T two)
```
Returns the smaller of two values.

### max
```d
T max<T>(T one, T two)
```
Returns the larger of two values.

## Rounding Functions

### floor
```d
double floor(double f)
float floor(float f)
half floor(half f)
```
Rounds down to nearest integer.

### ceil
```d
double ceil(double d)
float ceil(float f)
half ceil(half h)
```
Rounds up to nearest integer.

### round
```d
double round(double x, int n)
float round(float x, int n)
```
Rounds to n decimal places.

### mod
```d
double mod(double x)
float mod(float x)
half mod(half x)
```
Returns fractional part of a number.

## Exponential and Logarithmic

### exp
```d
double exp(double x)
float exp(float x)
```
Returns e raised to the power x.

### loge
```d
double loge(double x)
float loge(float x)
```
Natural logarithm (base e).

### log
```d
double log(double x)
float log(float x)
```
Common logarithm (base 10).

### pow
```d
double pow(double f, double f2)
float pow(float f, float f2)
```
Returns f raised to the power f2.

### sqrt
```d
double sqrt(double d)
float sqrt(float f)
half sqrt(half hf)
```
Square root.

### cbrt
```d
double cbrt(double d)
float cbrt(float f)
```
Cube root.

## Trigonometric Functions

### sin
```d
double sin(double x)
float sin(float x)
```
Sine function.

### cos
```d
double cos(double d)
float cos(float f)
```
Cosine function.

### tan
```d
double tan(double d)
float tan(float f)
```
Tangent function.

### asin
```d
double asin(double x)
float asin(float x)
```
Arcsine function.

### acos
```d
double acos(double d)
float acos(float f)
```
Arccosine function.

### degToRad
```d
double degToRad(double d)
float degToRad(float d)
half degToRad(half hf)
```
Converts degrees to radians.

### radToDeg
```d
double radToDeg(double r)
float radToDeg(float r)
half radToDeg(half r)
```
Converts radians to degrees.

## Hyperbolic Functions

### sinh
```d
double sinh(double x)
float sinh(float x)
```
Hyperbolic sine.

### cosh
```d
double cosh(double x)
float cosh(float x)
```
Hyperbolic cosine.

### tanh
```d
double tanh(double x)
float tanh(float x)
```
Hyperbolic tangent.

### asinh
```d
double asinh(double x)
float asinh(float x)
```
Inverse hyperbolic sine.

### acosh
```d
double acosh(double x)
float acosh(float x)
```
Inverse hyperbolic cosine.

### atanh
```d
double atanh(double x)
float atanh(float x)
```
Inverse hyperbolic tangent.

## Neural Network Functions

### sigmoid
```d
double sigmoid(double x)
float sigmoid(float x)
```
Sigmoid activation function: 1 / (1 + e^(-x))

### dsigmoid
```d
double dsigmoid(double x)
float dsigmoid(float x)
half dsigmoid(half x)
```
Derivative of sigmoid.

### relu
```d
double relu(double x)
float relu(float x)
```
ReLU activation: max(0, x)

### drelu
```d
double drelu(double x)
float drelu(float x)
half drelu(half x)
```
Derivative of ReLU.

### gelu
```d
double gelu(double x)
float gelu(float x)
half gelu(half x)
```
GELU activation function.

### geglu
```d
void geglu(double* input, double* output, int size)
void geglu(float* input, float* output, int size)
```
GeGLU activation for transformer models.

### softmax
```d
void softmax(double* x, int size)
void softmax(float* x, int size)
```
Softmax function (in-place).

### softplus
```d
double softplus(double x)
float softplus(float x)
```
Softplus activation: log(1 + e^x)

### rmsnorm
```d
void rmsnorm(double* out, double* x, double* weight, int size)
void rmsnorm(float* out, float* x, float* weight, int size)
```
RMS normalization for neural networks.

## Utility Functions

### fma
```d
double fma(double a, double b, double c)
float fma(float a, float b, float c)
half fma(half a, half b, half c)
```
Fused multiply-add: (a * b) + c

### isNAN
```d
bool isNAN(double d)
bool isNAN(float f)
bool isNAN(half hf)
```
Checks if value is Not-a-Number.

### factorial
```d
long factorial(int n)
```
Returns n! (factorial).

### isPrime
```d
bool isPrime(int number)
```
Checks if number is prime.

### erf
```d
double erf(double x)
float erf(float x)
```
Error function.

## Example

```d
import <std/math>

double angle = std::math::degToRad(45.0);
double result = std::math::sin(angle);
double power = std::math::pow(2.0, 8.0);
double root = std::math::sqrt(16.0);
```
