#ifndef CHOLESKY_H_
#define CHOLESKY_H_

/** The \c Cholesky decomposition is used to solve Ax = b; if A is symmetric and
 * positive definite => we can decompose A = LL* and instead of solving Ax = b,
 * solve Ly = b for y, and the solve L*x = y for x.
 */
class cholesky {
	//==========================================================================
	public:
		cholesky(){};
		virtual ~cholesky(){};

		/** Decomposes the (covariance) matrix A into A = LL*.
		 */
		int decomposeCov(cv::Mat a);

		/** Solves the general linear system: Ax = b and returns x.
		 */
		void solve(cv::Mat b, cv::Mat &x);

		/** Solve the simplified equation Ly = b, and return y (where A=LL*).
		 */
		void solveL(cv::Mat b, cv::Mat &y);

		/** Solve the simplified equation L'y = b, and return y (where A=LL*).
		 */
		void solveLTranspose(cv::Mat b, cv::Mat &y);

		/** Returns the log of the determiner of the (covariance) matrix, A.
		 */
		float logDet();
	//==========================================================================
	protected:
		unsigned n;
		cv::Mat_<float> covar;
};
#endif /* CHOLESKY_H_ */
