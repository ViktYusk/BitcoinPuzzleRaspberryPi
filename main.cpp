// TODO: посчитать, сколько раз выполняется каждая операция
// TODO: отчёт о найденных хешах
#include <chrono>
#include <iostream>
#include <mutex>
#include <pthread.h>

#include "Point.h"
#include "ripemd160.h"
#include "sha256.h"
#include "test.h"

using namespace std;
using namespace chrono;

const int BLOCK_BITS    = 26;
const int THREAD_BITS   = 2;
const int PROGRESS_BITS = 16;
const int SUBBLOCK_BITS = 63 - BLOCK_BITS - THREAD_BITS - PROGRESS_BITS - Key::GROUP_BITS;
const unsigned long long THREADS_NUMBER    = 1ULL << THREAD_BITS;
const unsigned long long PROGRESSES_NUMBER = 1ULL << PROGRESS_BITS;
const unsigned long long SUBBLOCKS_NUMBER  = 1ULL << SUBBLOCK_BITS;
const unsigned long long GROUPS_PER_PROGRESS = THREADS_NUMBER * SUBBLOCKS_NUMBER;

unsigned long long sha256Counter = 0;
unsigned long long ripemd160Counter = 0;

int code = 0;
int block;
int threadsProgresses[THREADS_NUMBER] = { 0 };
mutex mutex_;
Point threadsPoints[THREADS_NUMBER + 1];
Timer timer;

void* thread(void* id) 
{
	Point center(threadsPoints[*(int*)id]);
	unsigned char compression[64];
	memcpy(compression + 33, Point::COMPRESSION_ENDING, sizeof(Point::COMPRESSION_ENDING));
	for (int i = 0; i < PROGRESSES_NUMBER; i++)
	{
		for (int j = 0; j < SUBBLOCKS_NUMBER; j++)
		{
			Point points[Key::GROUP_SIZE + 1];
			center.group(points);
			for (int k = 0; k < Key::GROUP_SIZE; k++)
			{
				points[k].compress(compression);
				unsigned char sha256Output[32];
				sha256(compression, sha256Output);
				unsigned char address[20];
				ripemd160(sha256Output, address);
				if (Point::check(address))
				{
					mutex_.lock();
					code = 1;
					cout << points[k].key << endl;
					mutex_.unlock();
				}
			}
			center = points[Key::GROUP_SIZE];
		}
#ifdef DEBUG
		mutex_.lock();
		threadsProgresses[*(int*)id]++;
		bool log = true;
		for (int t = 0; t < THREADS_NUMBER; t++)
			log &= threadsProgresses[t] >= threadsProgresses[*(int*)id];
		if (log)
            cout << "Progress = " << (i + 1) * 100.0 / PROGRESSES_NUMBER << " % [" << (int)(GROUPS_PER_PROGRESS * Key::GROUP_SIZE / 1000 / timer.stop()) << " Kkeys/second]" << endl;
		mutex_.unlock();
#endif
	}
	if (!(center == threadsPoints[*(int*)id + 1]))
	{
		mutex_.lock();
		code = -1;
		cout << "Wrong finish point for thread # " << *(int*)id << endl;
		mutex_.unlock();
	}
	pthread_exit(0);
}

int main(int argc, char* argv[])
{
#ifdef DEBUG
	if (test())
		return -1;
#endif
	if (argc < 2)
	{
		cout << "No argument with block number" << endl;
		return -1;
	}
	for (int i = 0; argv[1][i] != 0; i++)
		if (argv[1][i] < '0' || argv[1][i] > '9')
		{
			cout << "Block number must be non-negative integer" << endl;
			return -1;
		}
	block = atoi(argv[1]);
	if (block < 0 || block >= 1 << BLOCK_BITS)
	{
		cout << "Block is not in range" << endl;
		return -1;
	}
	unsigned long long startKey = 1;
	startKey <<= BLOCK_BITS;
	startKey += block;
	startKey <<= THREAD_BITS;
	startKey <<= PROGRESS_BITS + SUBBLOCK_BITS + Key::GROUP_BITS;
	startKey += Key::GROUP_SIZE / 2;
	Point::initialize();
	threadsPoints[0] = Point(startKey);
	unsigned long long threadKeys = 1ULL << (PROGRESS_BITS + SUBBLOCK_BITS + Key::GROUP_BITS);
	unsigned long long threadKey = threadKeys;
	for (int i = 1; i <= THREADS_NUMBER; i++)
	{
		threadsPoints[i] = Point(threadKey);
		threadsPoints[i] += threadsPoints[0];
		threadKey += threadKeys;
	}
	timer = Timer();
	int thread_ids[THREADS_NUMBER];
	pthread_t threads[THREADS_NUMBER];
	for (int id = 0; id < THREADS_NUMBER; id++)
	{
		thread_ids[id] = id;
		if (pthread_create(&threads[id], NULL, thread, &thread_ids[id]) != 0)
		{
			cout << "Error while creating thread # " << id << endl;
			return -1;
		}
	}
	for (int id = 0; id < THREADS_NUMBER; id++)
		if (pthread_join(threads[id], NULL) != 0)
		{
			cout << "Error while joining thread" << endl;
			return -1;
		}
	return code;
}
