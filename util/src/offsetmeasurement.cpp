#include "offsetmeasurement.h"
#include "log.h"

#include <string>

OffsetMeasurement::OffsetMeasurement(int index, int64_t starttime_ns, int64_t endtime_ns,
                                     size_t samples_sent, size_t samples,
                                     size_t used, int64_t avg_ns, ResultCode resultCode)
    : m_index(index),
      m_starttime_ns(starttime_ns), m_endtime_ns(endtime_ns),
      m_sentSamples(samples_sent),
      m_collectedSamples(samples),
      m_usedSamples(used),
      m_offset_ns(avg_ns),
      m_resultCode(resultCode)
{
}


OffsetMeasurement::ResultCode OffsetMeasurement::resultCode() const
{
    return m_resultCode;
}


void OffsetMeasurement::setOffset_ns(int64_t offset_ns)
{
    m_clientOffset_ns = offset_ns;
}


std::string OffsetMeasurement::toString() const
{
    if (!m_collectedSamples)
    {
        trace->critical("no samples ?");
        return "no samples";
    }
    double used_samples = (100.0 * m_usedSamples) / m_collectedSamples;
    return fmt::format("# {} runtime {} samples/used {:5d}/{:4.1f}% offset_us {:-7.3f} ppm {:-7.3f} {}",
                       m_index, m_endtime_ns,
                       m_collectedSamples, used_samples,
                       m_offset_ns/1000.0, m_ppm,
                       m_resultCode == ResultCode::PASS ? "" : fmt::format("failed with code {}", m_resultCode));
}


double OffsetMeasurement::packageLossPct() const
{
    if (!m_sentSamples)
        return 0.0;
    return 100.0 - 100.0 * (m_sentSamples - m_collectedSamples) / m_sentSamples;
}
