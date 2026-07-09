#include "waveModels/RegularWave.hpp"
#include "addToRunTimeSelectionTable.H"
#include "pocketfft_eigen.hpp"
#include "error.H"
#include "vector.H"
#include <cmath>
#include <algorithm>

namespace Foam
{

defineTypeNameAndDebug(RegularWave, 0);
addToRunTimeSelectionTable(waveModel, RegularWave, dictionary);

RegularWave::RegularWave(const dictionary& dict)
:
    L_(dict.get<scalar>("waveLength")),
    D_(dict.get<scalar>("waterDepth")),
    H_(dict.get<scalar>("waveHeight")),
    waveCrestIni_(dict.getOrDefault<scalar>("waveCrestIni", 0.0))
{
    Foam::vector wDir = dict.getOrDefault<Foam::vector>("waveDirection", Foam::vector(1, 0, 0));
    waveDirX_ = wDir.x();
    waveDirY_ = wDir.y();

    k0_ = 2.0 * M_PI / L_;
    kd_ = k0_ * D_;
    nxp_ = nx_ + 4;
    nzp_ = nz_ + 4;
    computeSSGW();
    buildGrids();
}

autoPtr<waveModel> RegularWave::clone() const
{
    return autoPtr<waveModel>(new RegularWave(*this));
}

static void pchip_eval(const VectorXd& x, const VectorXd& y, const VectorXd& xi, VectorXd& yi)
{
    int n = x.size();
    VectorXd h(n-1), delta(n-1), d(n);
    for (int i = 0; i < n-1; i++) {
        h(i) = x(i+1) - x(i);
        delta(i) = (y(i+1) - y(i)) / h(i);
    }
    d(0) = delta(0);
    for (int i = 1; i < n-1; i++) {
        if (delta(i-1) * delta(i) > 0) {
            double w1 = 2*h(i) + h(i-1);
            double w2 = h(i) + 2*h(i-1);
            d(i) = (w1 + w2) / (w1/delta(i-1) + w2/delta(i));
        } else {
            d(i) = 0;
        }
    }
    d(n-1) = delta(n-2);

    int idx = 0;
    for (int k = 0; k < xi.size(); k++) {
        double xik = xi(k);
        while (idx < n-2 && x(idx+1) < xik) ++idx;
        double dx = xik - x(idx);
        double c0 = y(idx);
        double c1 = d(idx);
        double c2 = (3*delta(idx) - 2*d(idx) - d(idx+1)) / h(idx);
        double c3 = (d(idx) + d(idx+1) - 2*delta(idx)) / (h(idx)*h(idx));
        yi(k) = c0 + c1*dx + c2*dx*dx + c3*dx*dx*dx;
    }
}

void RegularWave::computeSSGW()
{
    using namespace Eigen;

    const double two_pi = 2.0 * M_PI;

    double kH2 = k0_ * H_ / 2.0;

    const int N = 16384;
    const int M = 2 * N;
    const double tol = 1e-7;
    const double inf = std::numeric_limits<double>::infinity();

    double d_val, k_val;
    bool deep;
    if (1.0 - tanh(kd_) < tol) {
        d_val = inf;
        k_val = 1.0;
        deep = true;
    } else {
        d_val = 1.0; 
        k_val = kd_ / d_val;
        deep = false;
    }
    double H = 2 * kH2 / k_val;
    double L = M_PI / k_val;
    double dal = L / N;
    double dk = M_PI / L;

    VectorXd va(M), vk_full(M);
    for (int i = 0; i < M; i++) va(i) = i * dal;
    for (int i = 0; i < N; i++) vk_full(i) = i * dk;
    for (int i = N; i < M; i++) vk_full(i) = (i - M) * dk;

    VectorXd vk_r2c(N+1);
    for (int i = 0; i <= N; i++) vk_r2c(i) = i * dk;
    VectorXcd vk_r2c_cplx = vk_r2c.cast<std::complex<double>>();

    VectorXd Ups(M);
    for (int i = 0; i < M; i++) Ups(i) = (H/2) * (1.0 + cos(k_val * va(i)));

    double sig = 1.0;
    double err = inf;
    double Bg = 0.0, mys = 0.0, del = 0.0;

    VectorXcd Ys_hat(N+1);
    VectorXd Ys(M), CUps(M), CUps2(M);
    VectorXd C_hat_r2c(N+1), Cinf_hat(N+1), CIC_hat(N+1), L_hat(N+1), IL_hat(N+1);
    VectorXd Ups2(M);

    int iter_count = 0;
    while (err > tol && iter_count < 2048) {
        iter_count++;
        double mUps = Ups.mean();
        Ys = Ups.array() - mUps;

        if (deep) {
            sig = 1.0;
            fft(Ys, Ys_hat);
            VectorXcd tmp = vk_r2c_cplx.array() * Ys_hat.array();
            VectorXd CYs(M);
            ifft(tmp, CYs);
            mys = -Ys.dot(CYs) / N / 2.0;
        } else {
            double sig_old = sig;
            fft(Ys, Ys_hat);
            for (int sub = 0; sub < 20; sub++) {
                double sd = sig * d_val;
                for (int i = 0; i <= N; i++) {
                    double sdk = sd * vk_r2c(i);
                    if (i == 0) C_hat_r2c(i) = 1.0 / sd;
                    else C_hat_r2c(i) = vk_r2c(i) / tanh(sdk);
                }
                VectorXcd C_hat_r2c_cplx = C_hat_r2c.cast<std::complex<double>>();
                VectorXcd tmp = C_hat_r2c_cplx.array() * Ys_hat.array();
                VectorXd inner(M);
                ifft(tmp, inner);
                double E = (Ys.array() * inner.array()).mean() + (sig - 1.0) * d_val;

                for (int i = 0; i <= N; i++) {
                    double sdk = sd * vk_r2c(i);
                    if (i == 0) C_hat_r2c(i) = 1.0 / (sd*sd);
                    else { double sch = 1.0 / sinh(sdk); C_hat_r2c(i) = (vk_r2c(i)*sch)*(vk_r2c(i)*sch); }
                }
                C_hat_r2c_cplx = C_hat_r2c.cast<std::complex<double>>();
                tmp = C_hat_r2c_cplx.array() * Ys_hat.array();
                ifft(tmp, inner);
                double dE = d_val - d_val * (Ys.array() * inner.array()).mean();
                sig = sig - E / dE;
                if (std::abs(sig - sig_old) < 1e-14) break;
                sig_old = sig;
            }
            mys = (sig - 1.0) * d_val;
        }

        del = mys - Ups.mean();

        Ups2 = Ups.array() * Ups.array();

        if (deep) {
            for (int i = 0; i <= N; i++) C_hat_r2c(i) = vk_r2c(i);
            C_hat_r2c(0) = 1.0 / (sig * d_val);
        } else {
            double sd = sig * d_val;
            for (int i = 0; i <= N; i++) {
                double sdk = sd * vk_r2c(i);
                if (i == 0) C_hat_r2c(i) = 1.0 / sd;
                else C_hat_r2c(i) = vk_r2c(i) / tanh(sdk);
            }
        }
        VectorXcd C_hat_r2c_cplx = C_hat_r2c.cast<std::complex<double>>();

        VectorXcd Ups_hat(N+1);
        fft(Ups, Ups_hat);
        VectorXcd tmp = C_hat_r2c_cplx.array() * Ups_hat.array();
        ifft(tmp, CUps);

        VectorXcd Ups2_hat(N+1);
        fft(Ups2, Ups2_hat);
        tmp = C_hat_r2c_cplx.array() * Ups2_hat.array();
        ifft(tmp, CUps2);

        double DCU = CUps(N) - CUps(0);
        double DCU2 = CUps2(N) - CUps2(0);
        double d_inv = deep ? 0.0 : 1.0/d_val;
        Bg = 2*del - H/sig * (1.0 + del*d_inv + sig*CUps(0)) / DCU + DCU2 / DCU / 2.0;

        double sd = deep ? inf : sig * d_val;
        for (int i = 0; i <= N; i++) {
            Cinf_hat(i) = vk_r2c(i);
            if (deep) CIC_hat(i) = 1.0;
            else CIC_hat(i) = tanh(sd * vk_r2c(i));
            if (i == 0) {
                L_hat(i) = 0.0;
                IL_hat(i) = 1.0;
            } else {
                L_hat(i) = (Bg - 2*del) * Cinf_hat(i) - (1.0 + del*d_inv) / sig * CIC_hat(i);
                IL_hat(i) = 1.0 / L_hat(i);
            }
        }
        VectorXcd Cinf_hat_cplx = Cinf_hat.cast<std::complex<double>>();
        VectorXcd CIC_hat_cplx = CIC_hat.cast<std::complex<double>>();
        VectorXcd L_hat_cplx = L_hat.cast<std::complex<double>>();
        VectorXcd IL_hat_cplx = IL_hat.cast<std::complex<double>>();

        VectorXcd LUps_hat = L_hat_cplx.array() * Ups_hat.array();
        VectorXd LUps(M);
        ifft(LUps_hat, LUps);

        tmp = C_hat_r2c_cplx.array() * Ups_hat.array();
        ifft(tmp, Ys);
        fft(Ups.array() * Ys.array(), tmp);
        VectorXcd NUps_hat = CIC_hat_cplx.array() * tmp.array();

        NUps_hat.array() += Cinf_hat_cplx.array() * Ups2_hat.array() / 2.0;

        VectorXd NUps(M);
        ifft(NUps_hat, NUps);
        double S = Ups.dot(LUps) / Ups.dot(NUps);

        VectorXcd tmp2(N+1);
        tmp2 = NUps_hat.array() * IL_hat_cplx.array();
        VectorXd U(M);
        ifft(tmp2, U);

        U = S * S * U.array();
        double U0 = U(0), UN = U(N);
        U = H * (U.array() - UN) / (U0 - UN);

        err = (U - Ups).cwiseAbs().maxCoeff();
        Ups = U;
    }

    double mUps = Ups.mean();
    Ys = Ups.array() - mUps;
    fft(Ys, Ys_hat);

    double sd_inf = deep ? inf : sig * d_val;

    VectorXcd IH_hat_cplx(N+1);
    for (int i = 0; i <= N; i++) {
        if (i == 0) IH_hat_cplx(0) = 0.0;
        else {
            double sdk = sd_inf * vk_r2c(i);
            IH_hat_cplx(i) = std::complex<double>(0.0, -1.0 / std::tanh(sdk));
        }
    }

    VectorXcd tmp(N+1);
    tmp = C_hat_r2c.cast<std::complex<double>>().array() * Ys_hat.array();
    VectorXd CYs(M);
    ifft(tmp, CYs);

    VectorXcd tmp2(N+1);
    tmp2 = IH_hat_cplx.array() * Ys_hat.array();
    VectorXd Xs(M);
    ifft(tmp2, Xs);

    mys = -Ys.dot(CYs) / N / 2.0;

    VectorXcd Zs(M), Zs_hat(M), dZs(M);
    for (int i = 0; i < M; i++) Zs(i) = std::complex<double>(Xs(i), Ys(i));
    fft_c2c(Zs, Zs_hat);
    for (int i = 0; i < M; i++) Zs_hat(i) *= std::complex<double>(0.0, vk_full(i));
    ifft_c2c(Zs_hat, dZs);

    VectorXd zs_re(M), zs_im(M), dzs_re(M), dzs_im(M);
    for (int i = 0; i < M; i++) {
        zs_re(i) = va(i) + Zs(i).real();
        zs_im(i) = mys + Zs(i).imag();
        dzs_re(i) = 1.0 + dZs(i).real();
        dzs_im(i) = dZs(i).imag();
    }

    VectorXd AB(M);
    for (int i = 0; i < M; i++) AB(i) = (1.0 + CYs(i)) / (dzs_re(i)*dzs_re(i) + dzs_im(i)*dzs_im(i));
    double ce = AB.sum() / 2.0 / N;
    ce = std::sqrt(Bg / ce);
    if(deep){
        ce_ = ce * std::sqrt(9.81 / k0_);
    }else{
        ce_ = ce * std::sqrt(9.81 / k0_ * kd_);
    }

    VectorXd va_new(M);
    for (int i = 0; i < M; i++) va_new(i) = i * (M_PI / k_val / N);
    VectorXd phi_lab(M);
    for (int i = 0; i < M; i++) phi_lab(i) = ce * (zs_re(i) - va_new(i));

    VectorXd xv(M);
    for (int i = 0; i < M; i++) xv(i) = zs_re(i) / two_pi;
    double coeff = xv(M-1) + xv(1) - xv(0);
    xv = xv.array() / coeff;
    double Lnow = L_ / coeff;

    VectorXd eta_v(M);
    for (int i = 0; i < M; i++) eta_v(i) = zs_im(i) * Lnow / two_pi;

    VectorXd phi(M);
    double pi15 = std::pow(two_pi, 1.5);
    double sqrtg = std::sqrt(9.81);
    double Lnow15 = std::pow(Lnow, 1.5);
    for (int i = 0; i < M; i++) phi(i) = phi_lab(i) / pi15 * sqrtg * Lnow15;

    int Nmodes = 2;
    VectorXd b_vec;
    err = 1.0;
    MatrixXd A(M, 2);
    while (err > 5e-4 && Nmodes <= 4096) {
        Nmodes *= 2;
        A.resize(M, Nmodes);
        for (int j = 0; j < Nmodes; j++) {
            int n = j + 1;
            for (int i = 0; i < M; i++) {
                double arg = two_pi * n * xv(i);
                double kd_n = n * kd_;
                A(i, j) = sin(arg) * (cosh(two_pi/L_ * n * eta_v(i))
                         + sinh(two_pi/L_ * n * eta_v(i)) * tanh(kd_n));
            }
        }
        b_vec = A.colPivHouseholderQr().solve(phi);
        VectorXd resid = A * b_vec - phi;
        err = resid.norm() / phi.norm();
    }

    int Nmodes_trim = 1;
    VectorXd bCalc = VectorXd::Zero(b_vec.size());
    bCalc(0) = b_vec(0);
    err = 1.0;
    while (err > 1e-3 && Nmodes_trim < b_vec.size()) {
        Nmodes_trim++;
        bCalc(Nmodes_trim-1) = b_vec(Nmodes_trim-1);
        VectorXd resid_trim = A * bCalc - phi;
        err = resid_trim.norm() / phi.norm();
    }

    b_ = bCalc.head(Nmodes_trim);

    VectorXd x_ext(3*M), eta_ext(3*M);
    for (int i = 0; i < M; i++) {
        x_ext(i) = xv(i) - 1.0;
        eta_ext(i) = eta_v(i);
        x_ext(M+i) = xv(i);
        eta_ext(M+i) = eta_v(i);
        x_ext(2*M+i) = xv(i) + 1.0;
        eta_ext(2*M+i) = eta_v(i);
    }

    VectorXd xi(8192);
    for (int i = 0; i < 8192; i++) xi(i) = double(i) / (8192.0 - 1.0);
    VectorXd eta_core(8192);
    pchip_eval(x_ext, eta_ext, xi, eta_core);
    double dx_eta = L_ / (8192.0 - 1.0);
    nxp_ = 8192 + 4;
    x_eta_.resize(nxp_);
    eta_.resize(nxp_);
    for (int i = 0; i < nxp_; i++)
        x_eta_(i) = (i - 2) * dx_eta;
    eta_.segment(2, 8192) = eta_core;
    eta_(0) = eta_core(8189);
    eta_(1) = eta_core(8190);
    eta_(8194) = eta_core(1);
    eta_(8195) = eta_core(2);
}

void RegularWave::buildGrids()
{
    using namespace Eigen;

    double dx = L_ / (nx_ - 1);
    xgrid_ = VectorXd::LinSpaced(nxp_, -2*dx, L_ + 2*dx);

    deff_ = std::min(D_, L_);
    double z_max = eta_.segment(2, nx_).maxCoeff();
    exp_top_ = std::exp(k0_ * (z_max + deff_));

    double dt = 1.0 / (nz_ - 1);
    zgrid_.resize(nzp_);
    for (int j = 0; j < nz_; j++) {
        double t = j * dt;
        zgrid_(j + 2) = std::log(1.0 + t * (exp_top_ - 1.0)) / k0_ - deff_;
    }
    for (int j = 0; j < 2; j++) {
        double t = 1.0 + (j + 1) * dt;
        zgrid_(nz_ + 2 + j) = std::log(1.0 + t * (exp_top_ - 1.0)) / k0_ - deff_;
    }
    zgrid_(1) = 2.0 * zgrid_(2) - zgrid_(3);
    zgrid_(0) = 2.0 * zgrid_(1) - zgrid_(2);

    Vx_.resize(nzp_, nxp_);
    Vz_.resize(nzp_, nxp_);
    Vx_.setZero();
    Vz_.setZero();

    int nmodes = b_.size();
    for (int n = 0; n < nmodes; n++) {
        double kn = (n + 1) * k0_;
        double kd_n = kn * D_;
        double scale = kn * b_(n);

        VectorXd cos_kx = (kn * xgrid_.array()).cos();
        VectorXd sin_kx = (kn * xgrid_.array()).sin();

        double exp_m2kd = std::exp(-2.0 * kd_n);
        for (int j = 2; j < nzp_; j++) {
            double exp_kz = std::exp(kn * zgrid_(j));
            double exp_m2kzd = std::exp(-2.0 * kn * (zgrid_(j) + D_));
            double denom = 1.0 / (1.0 + exp_m2kd);
            double Vxf = scale * exp_kz * (1.0 + exp_m2kzd) * denom;
            double Vzf = scale * exp_kz * (1.0 - exp_m2kzd) * denom;
            Vx_.row(j).array() += Vxf * cos_kx.array();
            Vz_.row(j).array() += Vzf * sin_kx.array();
        }
    }

    Vx_.row(1) = 2.0 * Vx_.row(2) - Vx_.row(3);
    Vx_.row(0) = 2.0 * Vx_.row(1) - Vx_.row(2);
    Vz_.row(1) = 2.0 * Vz_.row(2) - Vz_.row(3);
    Vz_.row(0) = 2.0 * Vz_.row(1) - Vz_.row(2);
}

double RegularWave::cubic1d(const VectorXd& x, const VectorXd& f, double xq) const
{
    double dx = x(1) - x(0);
    int i = int((xq - x(0)) / dx);

    double t = (xq - x(i)) / dx;
    double t2 = t * t, t3 = t2 * t;

    double Pm1 = f(i-1), P0 = f(i), P1 = f(i+1), P2 = f(i+2);
    return 0.5 * ((2.0*P0)
                + (-Pm1 + P1) * t
                + (2.0*Pm1 - 5.0*P0 + 4.0*P1 - P2) * t2
                + (-Pm1 + 3.0*P0 - 3.0*P1 + P2) * t3);
}

double RegularWave::bicubic2d(const VectorXd& x, const VectorXd& z,
                              const MatrixXd& F, double xq, double zq) const
{
    double dx = x(1) - x(0);
    int ix = int((xq - x(0)) / dx);
    ix = std::max(1, std::min(ix, nxp_ - 3));

    double dt = 1.0 / (nz_ - 1);
    double tq = (std::exp(k0_ * (zq + deff_)) - 1.0) / (exp_top_ - 1.0);
    int iz = int(std::floor(tq / dt)) + 2;
    iz = std::max(1, std::min(iz, nz_ + 1));

    double tx = (xq - x(ix)) / dx;
    double tx2 = tx * tx, tx3 = tx2 * tx;
    double tz = (tq - (iz - 2) * dt) / dt;
    double tz2 = tz * tz, tz3 = tz2 * tz;

    double vals[4];
    for (int jj = -1; jj <= 2; jj++) {
        int j = iz + jj;
        double Pm1 = F(j, ix-1), P0 = F(j, ix), P1 = F(j, ix+1), P2 = F(j, ix+2);
        vals[jj+1] = 0.5 * ((2.0*P0)
                          + (-Pm1 + P1) * tx
                          + (2.0*Pm1 - 5.0*P0 + 4.0*P1 - P2) * tx2
                          + (-Pm1 + 3.0*P0 - 3.0*P1 + P2) * tx3);
    }

    double Pm1 = vals[0], P0 = vals[1], P1 = vals[2], P2 = vals[3];
    return 0.5 * ((2.0*P0)
                + (-Pm1 + P1) * tz
                + (2.0*Pm1 - 5.0*P0 + 4.0*P1 - P2) * tz2
                + (-Pm1 + 3.0*P0 - 3.0*P1 + P2) * tz3);
}

void RegularWave::computeVel
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::MatrixXd> result
) const
{
    double norm = std::sqrt(waveDirX_ * waveDirX_ + waveDirY_ * waveDirY_);
    double dx = waveDirX_ / norm, dy = waveDirY_ / norm;

    int npts = points.cols();
    for (int i = 0; i < npts; i++) {
        double x_local = points(0, i) * dx + points(1, i) * dy;
        double x = x_local - ce_ * t_ - waveCrestIni_;
        double z = points(2, i);
        x = x - L_ * std::floor(x / L_);
        double u_local = bicubic2d(xgrid_, zgrid_, Vx_, x, z);
        result(0, i) = u_local * dx;
        result(1, i) = u_local * dy;
        result(2, i) = bicubic2d(xgrid_, zgrid_, Vz_, x, z);
    }
}

void RegularWave::computeEta
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::VectorXd> result
) const
{
    double norm = std::sqrt(waveDirX_ * waveDirX_ + waveDirY_ * waveDirY_);
    double dx = waveDirX_ / norm, dy = waveDirY_ / norm;

    int npts = points.cols();
    for (int i = 0; i < npts; i++) {
        double x_local = points(0, i) * dx + points(1, i) * dy;
        double x = x_local - ce_ * t_ - waveCrestIni_;
        x = x - L_ * std::floor(x / L_);
        result(i) = cubic1d(x_eta_, eta_, x);
    }
}

}
