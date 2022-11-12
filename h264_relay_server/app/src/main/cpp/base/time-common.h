#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace base {

#if defined(WIN32)
static inline double get_time() {
	LARGE_INTEGER t, f;
	QueryPerformanceCounter(&t);
	QueryPerformanceFrequency(&f);
	//return static_cast<double>(t.QuadPart) / static_cast<double>(f.QuadPart);
	return (double)(t.QuadPart) / (double)(f.QuadPart);
}

#else
	
static inline double get_time() {
	struct timeval t;
	struct timezone tzp;
	gettimeofday(&t, &tzp);
	return t.tv_sec + t.tv_usec * 1e-6;
}
#endif // #if defined(WIN32)

/*
static inline double get_time() {
	clock_t t = clock();
	return (dobule)t / CLOCKS_PER_SEC;
}
*/

}  // namespace base

/*
#if defined(WIN32)
static int gettimeofday(struct timeval* tp, int* tz)
{
	//DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, ("######%d gettimeofday. \n", __LINE__));

	LARGE_INTEGER tickNow;
	static LARGE_INTEGER tickFrequency;
	static BOOL tickFrequencySet = FALSE;
	if (tickFrequencySet == FALSE)
	{
		QueryPerformanceFrequency(&tickFrequency);
		tickFrequencySet = TRUE;
	}
	QueryPerformanceCounter(&tickNow);
	tp->tv_sec = (long)(tickNow.QuadPart / tickFrequency.QuadPart);
	tp->tv_usec = (long)(((tickNow.QuadPart % tickFrequency.QuadPart) * 1000000L) / tickFrequency.QuadPart);

	return 0;
}
#endif // #if defined(WIN32)
*/


/*
#include <chrono>

template<typename TimeT = std::chrono::milliseconds>
struct measure
{
    template<typename F, typename ...Args>
    static typename TimeT::rep execution(F func, Args&&... args)
    {
        auto start = std::chrono::system_clock::now();

        // Now call the function with all the parameters you need.
        func(std::forward<Args>(args)...);

        auto duration = std::chrono::duration_cast< TimeT>
            (std::chrono::system_clock::now() - start);

        return duration.count();
    }
};

struct functor
{
    int state;
    functor(int state) : state(state) {}
    void operator()() const
    {
        std::cout << "In functor run for ";
    }
};

void func()
{
    std::cout << "In function, run for " << std::endl;
}

int main() 
{
    // codes directly
    std::cout << measure<>::execution([&]() {
        // your code
    }) << " ms" << std::endl;

    // functor
    std::cout << measure<>::execution(functor(3)) << std::endl;

    // function
    std::cout << measure<>::execution(func);
}
*/

#ifdef __cplusplus
}  // extern "C"
#endif