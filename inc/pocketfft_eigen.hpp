#include "pockfft_hdronly.h"
#include <Eigen/Dense>
using namespace Eigen;

void fft(const VectorXd &in, VectorXcd &out);
void ifft(const VectorXcd &in, VectorXd &out);
void fft_c2c(const VectorXcd &in, VectorXcd &out);
void ifft_c2c(const VectorXcd &in, VectorXcd &out);
MatrixXcf fft2(const MatrixXcf &in);
MatrixXcf ifft2(const MatrixXcf &in);


using shape_t = pocketfft::shape_t;
using stride_t = pocketfft::stride_t;
using namespace Eigen;
void fft(const VectorXd &in, VectorXcd &out)
{
    using Scalar = double;
    using Complex = std::complex<Scalar>;
    const shape_t shape_{static_cast<size_t>(in.size())};
    const shape_t axes_{0};
    const stride_t stride_in{sizeof(Scalar)};
    const stride_t stride_out{sizeof(Complex)};
    pocketfft::r2c(shape_, stride_in, stride_out, axes_, pocketfft::FORWARD, in.data(), out.data(), static_cast<Scalar>(1.0 / in.size()));
}
void ifft(const VectorXcd &in, VectorXd &out)
{
    using Scalar = double;
    using Complex = std::complex<Scalar>;
    const shape_t shape_{static_cast<size_t>(out.size())};
    const shape_t axes_{0};
    const stride_t stride_in{sizeof(Complex)};
    const stride_t stride_out{sizeof(Scalar)};
    pocketfft::c2r(shape_, stride_in, stride_out, axes_, pocketfft::BACKWARD, in.data(), out.data(), static_cast<Scalar>(1));
}
void fft_c2c(const VectorXcd &in, VectorXcd &out)
{
    using Scalar = double;
    using Complex = std::complex<Scalar>;
    const shape_t shape_{static_cast<size_t>(in.size())};
    const shape_t axes_{0};
    const stride_t stride_{static_cast<ptrdiff_t>(sizeof(Complex))};
    pocketfft::c2c(shape_, stride_, stride_, axes_, pocketfft::FORWARD,
        in.data(), out.data(), static_cast<Scalar>(1.0));
}
void ifft_c2c(const VectorXcd &in, VectorXcd &out)
{
    using Scalar = double;
    using Complex = std::complex<Scalar>;
    const shape_t shape_{static_cast<size_t>(in.size())};
    const shape_t axes_{0};
    const stride_t stride_{static_cast<ptrdiff_t>(sizeof(Complex))};
    pocketfft::c2c(shape_, stride_, stride_, axes_, pocketfft::BACKWARD,
        in.data(), out.data(), static_cast<Scalar>(1.0 / in.size()));
}

MatrixXcf fft2(const MatrixXcf &in)
{
    using Scalar = float;
    using Complex = std::complex<Scalar>;
    MatrixXcf out(in.rows(), in.cols());
    size_t nx = in.rows();
    size_t ny = in.cols();
    const shape_t shape_{ny, nx};
    const shape_t axes_{0, 1};
    const stride_t stride_{static_cast<ptrdiff_t>(sizeof(Complex) * nx), static_cast<ptrdiff_t>(sizeof(Complex))};
    pocketfft::c2c(shape_, stride_, stride_, axes_, pocketfft::FORWARD, in.data(), out.data(), static_cast<Scalar>(1.0/in.size()));
    return out;
}
MatrixXcf ifft2(const MatrixXcf &in)
{
    using Scalar = float;
    using Complex = std::complex<Scalar>;
    MatrixXcf out(in.rows(), in.cols());
    size_t nx = in.rows();
    size_t ny = in.cols();
    const shape_t shape_{ny, nx};
    const shape_t axes_{0, 1};
    const stride_t stride_{static_cast<ptrdiff_t>(sizeof(Complex) * nx), static_cast<ptrdiff_t>(sizeof(Complex))};
    pocketfft::c2c(shape_, stride_, stride_, axes_, pocketfft::BACKWARD, in.data(), out.data(), static_cast<Scalar>(1.0));
    return out;
}
