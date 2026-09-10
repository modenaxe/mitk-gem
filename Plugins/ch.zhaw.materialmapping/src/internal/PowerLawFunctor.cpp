#include "PowerLawFunctor.h"

PowerLawFunctor::PowerLawFunctor()
    : cached_it(m_ParamMap.cend())
{
}

PowerLawFunctor::PowerLawFunctor(const PowerLawFunctor& other)
    : m_ParamMap(other.m_ParamMap),
      cached_it(m_ParamMap.cend())
{
}

PowerLawFunctor::PowerLawFunctor(PowerLawFunctor&& other) noexcept
    : m_ParamMap(std::move(other.m_ParamMap)),
      cached_it(m_ParamMap.cend())
{
    other.ResetCache();
}

PowerLawFunctor& PowerLawFunctor::operator=(const PowerLawFunctor& other)
{
    if (this != &other)
    {
        m_ParamMap = other.m_ParamMap;
        ResetCache();
    }

    return *this;
}

PowerLawFunctor& PowerLawFunctor::operator=(PowerLawFunctor&& other) noexcept
{
    if (this != &other)
    {
        m_ParamMap = std::move(other.m_ParamMap);
        ResetCache();
        other.ResetCache();
    }

    return *this;
}

void PowerLawFunctor::ResetCache() const
{
    cached_it = m_ParamMap.cend();
    cached_param = nullptr;
}

void PowerLawFunctor::AddPowerLaw(PowerLawParameters _p, double _upperBound) {
    m_ParamMap.insert(std::make_pair(_upperBound, _p));
    ResetCache();
}

std::ostream &operator<<(std::ostream &_out, const PowerLawFunctor &_f) {
    _out << "Power laws: " << std::endl;
    for (const auto &pair : _f.m_ParamMap) {
        _out << "[" << pair.first << "] E = " << pair.second.factor << " * rho ^ " << pair.second.exponent << " + " << pair.second.offset << std::endl;
    }
    return _out;
}
