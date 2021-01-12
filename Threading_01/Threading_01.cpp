#include "pch.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <Windows.h>

constexpr auto TESTCOUNTER = 50;

LARGE_INTEGER li;
LARGE_INTEGER liStart;
LARGE_INTEGER liEnd;

void blankThread() {
	QueryPerformanceCounter(&liEnd);
}

std::mt19937 random_number_engine(time(0));
std::vector<std::thread> threads;

int RandomNumberGenerator(int min, int max)
{
	std::uniform_int_distribution<int> distribution(min, max);
	return distribution(random_number_engine);
}

struct timeFrame {
	int m1rows, m1cols, m2rows, m2cols;
	double multiplicationTime;
	timeFrame(int m1r, int m1c, int m2r, int m2c, double time) 
		: m1rows(m1r), m1cols(m1c), m2rows(m2r), m2cols(m2c), multiplicationTime(time) {}
};

class Matrix {
	std::vector<int> data;
	std::size_t n_rows;
	std::size_t n_cols;
public:
	Matrix() : Matrix(0, 0) {}
	Matrix(std::size_t n_rows, std::size_t n_cols, bool fill = false) : data(n_rows * n_cols), n_rows(n_rows), n_cols(n_cols) {
		if (fill)fillMatrix();
	}

	std::size_t row_count() const { return n_rows; }
	std::size_t col_count() const { return n_cols; }

	int& operator()(std::size_t i, std::size_t j) { return data[i * n_cols + j]; }
	const int& operator()(std::size_t i, std::size_t j) const { return data[i * n_cols + j]; }

	void fillMatrix();
	void clearMatrix();
};

void Matrix::fillMatrix() {
	for (auto &i : data)
		i = RandomNumberGenerator(0, 99);
}
void Matrix::clearMatrix() {
	for (auto &i : data)
		i = 0;
}

void outputMatrix(Matrix &m) {
	std::cout << "\n==========\n";
	for (std::size_t i = 0; i < m.row_count(); ++i) {
		for (std::size_t j = 0; j < m.col_count(); ++j)
			std::cout << std::setw(2) << std::setfill('0') << m(i, j) << " ";
		std::cout << std::endl;
	}
	std::cout << "\n==========\n";
}

//non threaded
void multiplyMatrices(Matrix &m1, Matrix &m2, Matrix &res) {
	if (m1.col_count() == m2.row_count()) {		
		for (int r = 0; r < m1.row_count(); ++r) {
			for (int c = 0; c < m2.col_count(); ++c) {
				for (int i = 0; i < m1.col_count(); ++i) {
					res(r, c) += m1(r, i) * m2(i, c);
				}
			}
		}
	}
}

//divide rows over threads
void multiplyRows(Matrix &m1, Matrix &m2, Matrix &res, int startRow, int endRow) {
	for (int r = startRow; r < endRow; ++r) 
		for (int c = 0; c < m2.col_count(); ++c) 
			for (int i = 0; i < m1.col_count(); ++i) 
				res(r, c) += m1(r, i) * m2(i, c);			
}

void multiplyMatricesT1(Matrix &m1, Matrix &m2, Matrix &res, int thread_count) {
	if (m1.col_count() == m2.row_count()) {
		while (m1.row_count() < thread_count) --thread_count;

		int rowsPerThread = (m1.row_count() / thread_count);
		for (int i = 0; i < thread_count; ++i) {
			threads.at(i) = std::thread(multiplyRows, std::ref(m1), std::ref(m2), std::ref(res), i * rowsPerThread, (i * rowsPerThread) + rowsPerThread);
			std::cout << i * rowsPerThread << "-" << (i * rowsPerThread) + rowsPerThread << std::endl;
		}

		for (int i = 0; i < thread_count; ++i)
			threads.at(i).join();
	}
}

int main()
{	
	double PCFreq = 0.0;
	double aggregateTime = 0.0;
	std::string input_code;
	std::vector<timeFrame> savedTimes;

	if (!QueryPerformanceFrequency(&li))
		std::cout << "Performance counter failed.\n";

	PCFreq = double(li.QuadPart) / 1000.0;
	
	int threadCount = std::thread::hardware_concurrency();	
	//threadCount = 2; //override
	for (int i = 0; i < threadCount; ++i)
		threads.push_back(std::thread());

	int m_size = 512;
	const int upperLimit = 512;

	std::cout << "available threads: " << threadCount << "\n";
	while (m_size <= upperLimit) {
		aggregateTime = 0;
		//setup matrices
		Matrix m1, m2, result;
		std::cout << "matrix size: " << m_size << std::endl;
		m1 = Matrix(m_size, m_size);
		m2 = Matrix(m_size, m_size);
		result = Matrix(m_size, m_size);

		for (int i = 0; i < TESTCOUNTER; ++i) {
			m1.fillMatrix();
			m2.fillMatrix();
			result.clearMatrix();

			QueryPerformanceCounter(&liStart); //start timer
			//multiplyMatrices(m1, m2, result);
			multiplyMatricesT1(m1, m2, result, threadCount);
			QueryPerformanceCounter(&liEnd); //end timer

			double timeTaken = double(liEnd.QuadPart - liStart.QuadPart) / PCFreq;
			aggregateTime += timeTaken;
			std::cout << timeTaken << "\n";
		}

		double avg = aggregateTime / double(TESTCOUNTER);
		savedTimes.emplace_back(m1.row_count(), m1.col_count(), m2.row_count(), m2.col_count(), avg);

		std::cout << "total average: " << avg << "ms.\n";
		//std::cout << "'c' to continue, 'x' to see all averages and exit.\n=============================\n";
		m_size *= 2;
	}	

	std::cout << "All average times: \n";
	for (auto t : savedTimes)
		std::cout << t.m1rows << "x" << t.m1cols << ": " << t.multiplicationTime << std::endl;
	return 0;
}
