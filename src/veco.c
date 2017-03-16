#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "iio.h"

#include "fail.c"
#include "xmalloc.c"
#include "random.c"

static float float_sum(float *x, int n)
{
	double r = 0;
	for (int i = 0; i < n; i++)
		r += x[i];
	return r;
}

static float float_avg(float *x, int n)
{
	double r = 0;
	for (int i = 0; i < n; i++)
		r += x[i];
	return n?r/n:r;
}

static float float_mul(float *x, int n)
{
	double r = 1;
	for (int i = 0; i < n; i++)
		r *= x[i];
	return r;
}

static float float_min(float *x, int n)
{
	float r = INFINITY;
	for (int i = 0; i < n; i++)
		if (x[i] < r)
			r = x[i];
	return r;
}

static float float_max(float *x, int n)
{
	float r = -INFINITY;
	for (int i = 1; i < n; i++)
		if (x[i] > r)
			r = x[i];
	return r;
}

int compare_floats(const void *a, const void *b)
{
	const float *da = (const float *) a;
	const float *db = (const float *) b;
	return (*da > *db) - (*da < *db);
}

static float float_med(float *x, int n)
{
	if (!n) return NAN;//fail("empty list of pixel values!");
	if (n == 1) return x[0];
	if (n == 2) return x[0];
	qsort(x, n, sizeof*x, compare_floats);
	return x[n/2];
}

static float float_cnt(float *x, int n)
{
	return n;
}

static float float_mod(float *x, int n)
{
	float h[0x100];
	for (int i = 0; i < 0x100; i++)
		h[i] = 0;
	for (int i = 0; i < n; i++)
	{
		int xi = x[i];
		if (xi < 0) fail("negative xi=%g", x[i]);//xi = 0;
		if (xi > 0xff) fail("large xi=%g", x[i]);//xi = 0xff;
		h[xi] += 2;
		if (xi > 0) h[xi-1] += 1;
		if (xi < 0xff) h[xi+1] += 1;
	}
	int mi = 0x80;
	for (int i = 0; i < 0x100; i++)
		if (h[i] > h[mi])
			mi = i;
	return mi;
}

#ifndef EVENP
#define EVENP(x) (!((x)&1))
#endif

static float float_medv(float *x, int n)
{
	if (!n) fail("empty list of pixel values!");
	if (n == 1) return x[0];
	if (n == 2) return (x[0] + x[1])/2;
	qsort(x, n, sizeof*x, compare_floats);
	if (EVENP(n))
		return (x[n/2] + x[1+n/2])/2;
	else
		return x[n/2];
}

static float float_percentile(float *x, int n)
{
	static float percentile = 50;
	if (n == -1) { percentile = fmax(0, fmin(*x, 100)); return 0; }
	if (n == 0) return NAN;
	if (n == 1) return x[0];
	qsort(x, n, sizeof*x, compare_floats);
	int i = round(percentile * (n - 1) / 100.0);
	//fprintf(stderr, "n=%d, percentile=%g, i=%d\n", n, percentile, i);
	assert(i >= 0);
	assert(i < n);
	return x[i];
}

static float float_first(float *x, int n)
{
	if (n)
		return x[0];
	else
		fail("empty list of pixel values!");
}

static float float_pick(float *x, int n)
{
	if (n) {
		int i = randombounds(0, n-1);
		return x[i];
	}
	else
		fail("empty list of pixel values!");
}

typedef bool (*isgood_t)(float);

static bool isgood_finite(float x) { return isfinite(x); }
static bool isgood_always(float x) { return true; }
static bool isgood_numeric(float x) { return !isnan(x); }
static bool isgood_nonzero(float x) { return x != 0; }
static bool isgood_positive(float x) { return x >= 0; }
static bool isgood_negative(float x) { return x < 0; }

static char *help_string_name     = "veco";
static char *help_string_version  = "veco 1.0\n\nWritten by eml";
static char *help_string_oneliner = "combine several scalar images into one";
static char *help_string_usage    = "usage:\n\t"
"veco {sum|min|max|mul|med|...} in1 in2 ... {> out|-o out}";
static char *help_string_long     =
"Veco combines several scalar images by a pixelwise operation\n"
"\n"
"Usage: veco OPERATION in1 in2 in3 ... > out\n"
"   or: veco OPERATION in1 in2 in3 ... -o out\n"
"\n"
"Options:\n"
" -o file      use a named output file instead of stdout\n"
" -g GOODNESS  use a specific GOODNESS criterion for discarding samples\n"
"\n"
"Operations:\n"
" min          minimum value of the good samples\n"
" max          maximum value of the good samples\n"
" avg          average of the good samples\n"
" sum          sum of the good samples\n"
" med          medoid of the good samples (the central sample, rounding down)\n"
" medv         median of the good samples (average of 1 or 2 central samples)\n"
" mod          mode of the good samples (the value that appears more times)\n"
" cnt          number of good samples\n"
" mul,prod     product of all good samples\n"
" first        the first good sample\n"
" rnd          a randomly chosen good sample\n"
"Goodness criteria:\n"
" numeric      whether the sample is not NAN, this is the default\n"
" finite       whether the sample is a finite number\n"
" always       consider all samples regardless of their value\n"
" nonzero      whether the sample is numeric and not 0 or -0\n"
" positive     whether the sample is >= 0\n"
"\n"
"Examples:\n"
" veco avg i*.png -o avg.png     Compute the average of a bunch of images\n"
"\n"
"Report bugs to <enric.meinhardt@cmla.ens-cachan.fr>."
;
#include "help_stuff.c"
#include "pickopt.c"
int main_veco(int c, char *v[])
{
	if (c == 2) if_help_is_requested_print_it_and_exit_the_program(v[1]);
	char *goodness =     pick_option(&c, &v, "g", "numeric");
	char *filename_out = pick_option(&c, &v, "o", "-");
	if (c < 3) {
		fprintf(stderr,
		"usage:\n\t%s {sum|min|max|avg|mul|med} [v1 ...] > out\n", *v);
		//          0  1                          2  3
		return EXIT_FAILURE;
	}
	int n = c - 2;
	char *operation_name = v[1];
	float (*f)(float *,int) = NULL;
	if (0 == strcmp(operation_name, "sum"))   f = float_sum;
	if (0 == strcmp(operation_name, "mul"))   f = float_mul;
	if (0 == strcmp(operation_name, "prod"))  f = float_mul;
	if (0 == strcmp(operation_name, "avg"))   f = float_avg;
	if (0 == strcmp(operation_name, "min"))   f = float_min;
	if (0 == strcmp(operation_name, "max"))   f = float_max;
	if (0 == strcmp(operation_name, "med"))   f = float_med;
	if (0 == strcmp(operation_name, "mod"))   f = float_mod;
	if (0 == strcmp(operation_name, "cnt"))   f = float_cnt;
	if (0 == strcmp(operation_name, "medi"))   f = float_med;
	if (0 == strcmp(operation_name, "medv"))   f = float_medv;
	if (0 == strcmp(operation_name, "rnd"))   f = float_pick;
	if (0 == strcmp(operation_name, "first")) f = float_first;
	if (*operation_name == 'q') {
		float p = atof(1 + operation_name);
		f = float_percentile;
		f(&p, -1);
	}
	if (!f) fail("unrecognized operation \"%s\"", operation_name);
	bool (*isgood)(float) = NULL;
	if (0 == strcmp(goodness, "finite"))   isgood = isgood_finite;
	if (0 == strcmp(goodness, "numeric"))  isgood = isgood_numeric;
	if (0 == strcmp(goodness, "always"))   isgood = isgood_always;
	if (0 == strcmp(goodness, "nonzero"))  isgood = isgood_nonzero;
	if (0 == strcmp(goodness, "positive")) isgood = isgood_positive;
	if (!isgood) fail("unrecognized goodness \"%s\"", goodness);
	float *x[n];
	int w[n], h[n];
	for (int i = 0; i < n; i++)
		x[i] = iio_read_image_float(v[i+2], w + i, h + i);
	for (int i = 0; i < n; i++) {
		if (w[i] != *w || h[i] != *h)
			fail("%dth image size mismatch\n", i);
	}
	float (*y) = xmalloc(*w * *h * sizeof*y);
	for (int i = 0; i < *w * *h; i++)
	{
		float tmp[n];
		int ngood = 0;
		for (int j = 0; j < n; j++)
			if (isgood(x[j][i]))
				tmp[ngood++] = x[j][i];
		y[i] = f(tmp, ngood);
	}
	iio_write_image_float(filename_out, y, *w, *h);
	return EXIT_SUCCESS;
}

#ifndef HIDE_ALL_MAINS
int main(int c, char **v) { return main_veco(c, v); }
#endif
