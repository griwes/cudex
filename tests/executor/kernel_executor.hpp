#include <cassert>
#include <cudex/executor/kernel_executor.hpp>
#include <cudex/property/blocking.hpp>
#include <cudex/property/bulk_guarantee.hpp>


namespace ns = cudex;


#ifndef __host__
#define __host__
#endif

#ifndef __device__
#define __device__
#endif

#ifndef __managed__
#define __managed__
#endif

#ifndef __global__
#define __global__
#endif


__managed__ int result;


__host__ __device__
void test_equality(cudaStream_t s, std::size_t dynamic_shared_memory_size, int d)
{
  using namespace ns;

  kernel_executor ex1{s, d};

  kernel_executor ex2 = ex1;

  assert(ex1 == ex2);
  assert(!(ex1 != ex2));
}


__host__ __device__
void test_properties(cudaStream_t s, std::size_t smem_size, int d)
{
  using namespace ns;

  kernel_executor ex{s, smem_size, d};

  assert(s == ex.stream());
  assert(smem_size == ex.dynamic_shared_memory_size());
  assert(d == ex.device());
  assert(bulk_guarantee.scoped(bulk_guarantee.parallel, bulk_guarantee.concurrent) == ex.query(bulk_guarantee_t{}));
  assert(blocking.possibly == ex.query(blocking));

  {
    // require dynamic_shared_memory_size
    std::size_t expected = 16;
    kernel_executor ex1 = ex.require(dynamic_shared_memory_size(expected));
    assert(s == ex1.stream());
    assert(s == ex.stream());
    assert(expected == ex.dynamic_shared_memory_size());
    assert(d == ex.device());
    assert(bulk_guarantee.scoped(bulk_guarantee.parallel, bulk_guarantee.concurrent) == ex.query(bulk_guarantee_t{}));
  }
}


__host__ __device__
void test_execute(cudaStream_t s, int d)
{
  using namespace ns;

  kernel_executor ex1{s, d};

  result = 0;
  int expected = 13;

  ex1.execute([=] __device__
  {
    result = expected;
  });

#ifndef __CUDA_ARCH__
  assert(cudaStreamSynchronize(s) == cudaSuccess);
#else
  assert(cudaDeviceSynchronize() == cudaSuccess);
#endif

  assert(expected == result);
}


__host__ __device__
unsigned int hash_coord(ns::kernel_executor::coordinate_type coord)
{
  return coord.block.x ^ coord.block.y ^ coord.block.z ^ coord.thread.x ^ coord.thread.y ^ coord.thread.z;
}


// this array has blockIdx X threadIdx axes
// put 4 elements in each axis
__managed__ unsigned int bulk_result[4][4][4][4][4][4] = {};


__host__ __device__
void test_bulk_execute(cudaStream_t s, int d)
{
  using namespace ns;

  kernel_executor ex1{s, d};

  kernel_executor::coordinate_type shape{::dim3(4,4,4), ::dim3(4,4,4)};

  ex1.bulk_execute([=] __device__ (kernel_executor::coordinate_type coord)
  {
    unsigned int result = hash_coord(coord);

    bulk_result[coord.block.x][coord.block.y][coord.block.z][coord.thread.x][coord.thread.y][coord.thread.z] = result;
  }, shape);

#ifndef __CUDA_ARCH__
  assert(cudaStreamSynchronize(s) == cudaSuccess);
#else
  assert(cudaDeviceSynchronize() == cudaSuccess);
#endif

  for(unsigned int bx = 0; bx != shape.block.x; ++bx)
  {
    for(unsigned int by = 0; by != shape.block.y; ++by)
    {
      for(unsigned int bz = 0; bz != shape.block.z; ++bz)
      {
        for(unsigned int tx = 0; tx != shape.thread.x; ++tx)
        {
          for(unsigned int ty = 0; ty != shape.thread.y; ++ty)
          {
            for(unsigned int tz = 0; tz != shape.thread.z; ++tz)
            {
              kernel_executor::coordinate_type coord{{bx,by,bz}, {tx,ty,tz}};
              unsigned int expected = hash_coord(coord);

              assert(expected == bulk_result[coord.block.x][coord.block.y][coord.block.z][coord.thread.x][coord.thread.y][coord.thread.z]);
            }
          }
        }
      }
    }
  }
}


__host__ __device__
void test_on_default_stream()
{
  test_execute(cudaStream_t{}, 0);
  test_bulk_execute(cudaStream_t{}, 0);
}


__host__ __device__
void test_on_new_stream()
{
  cudaStream_t s{};
  assert(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking) == cudaSuccess);

  test_execute(s, 0);
  test_bulk_execute(s, 0);

  assert(cudaStreamDestroy(s) == cudaSuccess);
}


#ifdef __CUDACC__
template<class F>
__global__ void device_invoke(F f)
{
  f();
}
#endif


void test_kernel_executor()
{
#ifdef __CUDACC__
  test_equality(cudaStream_t{}, 16, 0);
  test_properties(cudaStream_t{}, 16, 0);
  test_on_default_stream();
  test_on_new_stream();

  device_invoke<<<1,1>>>([] __device__ ()
  {
    test_equality(cudaStream_t{}, 16, 0);
    test_properties(cudaStream_t{}, 16, 0);
    test_on_default_stream();
    test_on_new_stream();
  });
  assert(cudaDeviceSynchronize() == cudaSuccess);
#endif
}

