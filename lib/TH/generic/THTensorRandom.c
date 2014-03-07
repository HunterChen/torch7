#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/THTensorRandom.c"
#else

TH_API void THTensor_(random)(THTensor *self)
{
#if defined(TH_REAL_IS_BYTE)
  TH_TENSOR_APPLY(real, self, *self_data = (unsigned char)(THRandom_random() % (UCHAR_MAX+1)););
#elif defined(TH_REAL_IS_CHAR)
  TH_TENSOR_APPLY(real, self, *self_data = (char)(THRandom_random() % (CHAR_MAX+1)););
#elif defined(TH_REAL_IS_SHORT)
  TH_TENSOR_APPLY(real, self, *self_data = (short)(THRandom_random() % (SHRT_MAX+1)););
#elif defined(TH_REAL_IS_INT)
  TH_TENSOR_APPLY(real, self, *self_data = (int)(THRandom_random() % (INT_MAX+1UL)););
#elif defined(TH_REAL_IS_LONG)
  TH_TENSOR_APPLY(real, self, *self_data = (long)(THRandom_random() % (LONG_MAX+1UL)););
#elif defined(TH_REAL_IS_FLOAT)
  TH_TENSOR_APPLY(real, self, *self_data = (float)(THRandom_random() % ((1UL << FLT_MANT_DIG)+1)););
#elif defined(TH_REAL_IS_DOUBLE)
  TH_TENSOR_APPLY(real, self, *self_data = (float)(THRandom_random() % ((1UL << DBL_MANT_DIG)+1)););
#else
#error "Unknown type"
#endif
}

TH_API void THTensor_(geometric)(THTensor *self, double p)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_geometric(p););
}

TH_API void THTensor_(bernoulli)(THTensor *self, double p)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_bernoulli(p););
}

#if defined(TH_REAL_IS_FLOAT) || defined(TH_REAL_IS_DOUBLE)

TH_API void THTensor_(uniform)(THTensor *self, double a, double b)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_uniform(a, b););
}

TH_API void THTensor_(normal)(THTensor *self, double mean, double stdv)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_normal(mean, stdv););
}

TH_API void THTensor_(exponential)(THTensor *self, double lambda)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_exponential(lambda););
}

TH_API void THTensor_(cauchy)(THTensor *self, double median, double sigma)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_cauchy(median, sigma););
}

TH_API void THTensor_(logNormal)(THTensor *self, double mean, double stdv)
{
  TH_TENSOR_APPLY(real, self, *self_data = (real)THRandom_logNormal(mean, stdv););
}

TH_API void THTensor_(multinomial)(THLongTensor *self, THTensor *prob_dist, int n_sample, int with_replacement)
{
  int start_dim = THTensor_(nDimension)(prob_dist);
  long n_dist;
  long n_categories;
  THTensor* cum_dist;
  int i,j,k;
  
  if (start_dim == 1)
  {
    THTensor_(resize2d)(prob_dist, 1, THTensor_(size)(prob_dist, 0));
  }
  
  n_dist = THTensor_(size)(prob_dist, 0);
  n_categories = THTensor_(size)(prob_dist, 1);
  
  THArgCheck(n_sample > 0, 2, "cannot sample n_sample < 0 samples");
  
  if (!with_replacement)
  {
    THArgCheck((!with_replacement) && (n_sample <= n_categories), 2, \
    "cannot sample n_sample > prob_dist:size(1) samples without replacement");
  }
  
  /* cumulative probability distribution vector */
  cum_dist = THTensor_(newWithSize1d)(n_categories);
    
  /* will contain multinomial samples (category indices to be returned) */
  THLongTensor_resize2d(self, n_dist , n_sample);
  
  for (i=0; i<n_dist; i++)
  {
    /* Get normalized cumulative distribution from prob distribution */
    real sum = 0;
    for (j=0; j<n_categories; j++)
    {
      sum += THStorage_(get)( \
        prob_dist->storage, \
        prob_dist->storageOffset+i*prob_dist->stride[0]+j*prob_dist->stride[1] \
      );
      THStorage_(set)( 
        cum_dist->storage, \
        cum_dist->storageOffset+j*cum_dist->stride[0], \
        sum \
      );
    }
    THArgCheck((sum > 0), 2, "invalid multinomial distribution (sum of probabilities <= 0)");
    /* normalize cumulative probability distribution so that last val is 1 
    i.e. dosen't assume original prob_dist row sums to one */
    if ( (sum > 0) || ( ( sum < 1.00001) && (sum > 0.99999) ) )  
    {
      for (j=0; j<n_categories; j++)
      {
        THTensor_(data)(cum_dist)[j*cum_dist->stride[0]] /= sum;
      }
    }
    
    for (j=0; j<n_sample; j++)
    {
      /* sample a probability mass from a uniform distribution */
      double uniform_sample = THRandom_uniform(0, 1);      
      /* Do a binary search for the slot in which the prob falls  
      ie cum_dist[row][slot-1] < uniform_prob < cum_distr[row][slot] */
      int left_pointer = 0;
      int right_pointer = n_categories;
      int mid_pointer;
      real cum_prob;
      int sample_idx;
      
      while(right_pointer - left_pointer > 0)
      {
          mid_pointer = left_pointer + (right_pointer - left_pointer) / 2;
          cum_prob = THStorage_(get)( \
            cum_dist->storage, \
            cum_dist->storageOffset+mid_pointer*cum_dist->stride[0] \
          );
          if (cum_prob < uniform_sample) 
          {
            left_pointer = mid_pointer + 1;
          }
          else
          {
            right_pointer = mid_pointer;
          }
      }
      sample_idx = left_pointer;
      
       /* store in result tensor (will be incremented for lua compat by wrapper) */
      THLongStorage_set( \
        self->storage, \
        self->storageOffset+i*self->stride[0]+j*self->stride[1], \
        sample_idx \
      );
      
      /* Once a sample is drawn, it cannot be drawn again. ie sample without replacement */
      if (!with_replacement)
      {
        /* update cumulative distribution so that sample cannot be drawn again */
        real diff;
        real new_val = 0;
        real sum;
        
        if (sample_idx != 0)
        {
          new_val = THStorage_(get)( \
            cum_dist->storage, \
            cum_dist->storageOffset+(sample_idx-1)*cum_dist->stride[0] \
          );
        }
        /* marginal cumulative mass (i.e. original probability) of sample */
        diff = THStorage_(get)( \
          cum_dist->storage, \
          cum_dist->storageOffset+sample_idx*cum_dist->stride[0] \
        ) - new_val;
        /* new sum of marginals is not one anymore... */
        sum = 1.0 - diff;
        for (k=0; k<n_categories; k++)
        {
          new_val = THStorage_(get)( \
            cum_dist->storage, \
            cum_dist->storageOffset+k*cum_dist->stride[0] \
          );
          if (k >= sample_idx) 
          {
            /* remove sampled probability mass from later cumulative probabilities */
            new_val -= diff;
          }
          /* make total marginals sum to one */
          new_val /= sum;
          THStorage_(set)( \
            cum_dist->storage, \
            cum_dist->storageOffset+k*cum_dist->stride[0], \
            new_val \
          );
        }
      }                                
    }
  }
  
  THTensor_(free)(cum_dist);
  
  if (start_dim == 1)
  {
    THLongTensor_resize1d(self, n_sample);
    THTensor_(resize1d)(prob_dist, n_categories);
  }
}

#endif

#if defined(TH_REAL_IS_LONG)
TH_API void THTensor_(getRNGState)(THTensor *self)
{
  unsigned long *data;
  long *offset;
  long *left;

  THTensor_(resize1d)(self,626);
  data = (unsigned long *)THTensor_(data)(self);
  offset = (long *)data+624;
  left = (long *)data+625;

  THRandom_getState(data,offset,left);
}

TH_API void THTensor_(setRNGState)(THTensor *self)
{
  unsigned long *data;
  long *offset;
  long *left;

  THArgCheck(THTensor_(nElement)(self) == 626, 1, "state should have 626 elements");
  data = (unsigned long *)THTensor_(data)(self);
  offset = (long *)(data+624);
  left = (long *)(data+625);

  THRandom_setState(data,*offset,*left);
}
#endif

#endif
