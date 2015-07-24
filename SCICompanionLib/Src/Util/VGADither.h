#pragma once

template <typename T>
class dither_traits
{
public:
};

struct RGBError
{
    RGBError() : r(0), g(0), b(0) {}
    int16_t r;
    int16_t g;
    int16_t b;

    RGBError operator*(int16_t m)
    {
        RGBError error = *this;
        error.r *= m;
        error.g *= m;
        error.b *= m;
        return error;
    }
    RGBError &operator+=(const RGBError &error)
    {
        r += error.r;
        g += error.g;
        b += error.b;
        return *this;
    }
};

template<>
class dither_traits<RGBQUAD>
{
public:
    typedef RGBError error_type;
};

template<>
class dither_traits<uint8_t>
{
public:
    typedef int16_t error_type;
};

struct OffsetAndWeight
{
    int x;
    int y;
    int weight;
};

class FloydSteinberg
{
public:
    static const int ExpandX = 2;
    static const int ExpandY = 1;
    static const int Divisor = 16;
    static const OffsetAndWeight Matrix[4];
};


class JarvisJudiceNinke
{
public:
    static const int ExpandX = 4;
    static const int ExpandY = 2;
    static const int Divisor = 42;
    static const OffsetAndWeight Matrix[12];
};


uint8_t ClampTo8(int16_t in);
RGBError CalculateError(RGBQUAD rgbOrig, RGBQUAD rgbChosen);
int16_t CalculateError(uint8_t orig, uint8_t chosen);
RGBQUAD AdjustWithError(RGBQUAD orig, RGBError accError, int16_t factor);
uint8_t AdjustWithError(uint8_t orig, int16_t accError, int16_t factor);

template<typename T>
class Dither
{
public:
    typedef typename dither_traits<T>::error_type TError;
    typedef FloydSteinberg Algorithm;
    //typedef JarvisJudiceNinke Algorithm;

    Dither(int cx, int cy)
    {
        _cxE = cx + Algorithm::ExpandX;   // Room for below left and right
        _cyE = cy + Algorithm::ExpandY;   // Room for below
        _error = std::make_unique<TError[]>(_cxE * _cyE);
    }

    T ApplyErrorAt(T rgb, int x, int y)
    {
        TError errorAccum = _error[y * _cxE + x];
        return AdjustWithError(rgb, errorAccum, Algorithm::Divisor);
    }

    void PropagateError(T rgbOrig, T rgbChosen, int x, int y)
    {
        // Propagate the error to nearby pixels using the floyd-steinberg algorithm
        TError errorBase = CalculateError(rgbOrig, rgbChosen);

        for (auto &offsetAndWeight : Algorithm::Matrix)
        {
            _error[(y + offsetAndWeight.y) * _cxE + x + offsetAndWeight.x] += errorBase * offsetAndWeight.weight;
        }
    }

private:
    std::unique_ptr<TError[]> _error;
    int _cxE;
    int _cyE;
};
