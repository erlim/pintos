#define F (1<<14)
#define INT_MAX ((1<<31)-1)
#define INT_MIN (-(1<<31))
// x and y deonote fices_point numbers in 17.14 format
//n is an integer

int int_to_fp(int n) ;
int fp_to_int_round(int x) ;
int fp_to_int(int x);
int add_fp(int x, int y) ;
int add_mixed(int x , int n) ;
int sub_fp(int x, int y );
int sub_mixed(int x , int n) ;
int mult_fp( int x , int y) ;
int mult_mixed(int x , int n) ;
int div_fp( int x , int y);
int div_mixed( int x, int n) ;

/* implementation function */

int int_to_fp(int n)
{
  int fixed_num = n * F ;
  return fixed_num ;
}
int fp_to_int_round( int x )
{
  int fixed_num = x / F ;
  return fixed_num ;
}
int fp_to_int( int x)
{
  int fixed_num;
  if(fixed_num >= 0 )
  {
    fixed_num= (x + F/2)/F;
  }
  else
  {
    fixed_num = (x - F/2)/F ;
  }
  return fixed_num ;
}
int add_fp( int x , int y)
{
  return x + y ;
}
int add_mixed(int x , int n)
{
  return x + n*F ;
}
int sub_fp( int x , int y)
{
  return x-y ;
}
int sub_mixed( int x , int n)
{
  x - n*F;
}
int mult_fp(int x , int y)
{
  return ((int64_t)x) * y / F;
}
int mult_mixed(int x , int n)
{
  return x * n ;
}
int div_fp(int x , int y)
{
  return ((int64_t)x)*F/y;
}
int div_mixed(int x , int n)
{
  return x/n ;
}
