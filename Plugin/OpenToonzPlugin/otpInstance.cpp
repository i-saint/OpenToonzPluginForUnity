#include "pch.h"
#include "Foundation.h"
using namespace utj;

#include "OpenToonzPlugin.h"
#include "otpModule.h"
#include "otpInstance.h"
#include "otpPort.h"
#include "otpParam.h"


otpPluginInfo otpProbeToInfo(toonz_plugin_probe_t *probe);

otpInstance::otpInstance(otpModule *module, toonz_plugin_probe_t *probe)
    : m_module(module)
    , m_probe(probe)
    , m_info(otpProbeToInfo(probe))
    , m_userdata()
    , m_base_width()
    , m_base_height()
    , m_frame()
    , m_canceled()
{
    m_dst_image.reset(new ImageRGBAu8());
    if (m_probe->handler) {
        if (m_probe->handler->setup) {
            m_probe->handler->setup(this);
        }
        if (m_probe->handler->create) {
            m_probe->handler->create(this);
        }
    }
}

otpInstance::~otpInstance()
{
    if (m_probe->handler) {
        if (m_probe->handler->destroy) {
            m_probe->handler->destroy(this);
        }
    }
}

void otpInstance::setParamInfo(toonz_param_page_t *pages, int num_pages)
{
    m_params.clear();
    for (int pi = 0; pi < num_pages; ++pi) {
        toonz_param_page_t& page = pages[pi];
        for (int gi = 0; gi < page.num; ++gi) {
            toonz_param_group_t& group = page.array[gi];
            for (int i = 0; i < group.num; ++i) {
                toonz_param_desc_t& desc = group.array[i];

                auto *param = new otpParam(this, desc);
                m_params.emplace_back(otpParamPtr(param));
            }
        }
    }
}

void otpInstance::addPort(const char *name)
{
    m_ports.emplace_back(otpPortPtr(new otpPort(this, name)));
}

const otpPluginInfo& otpInstance::getPluginInfo() const
{
    return m_info;
}


int otpInstance::getNumParams() const
{
    return (int)m_params.size();
}
otpParam* otpInstance::getParam(int i)
{
    return i < m_params.size() ? m_params[i].get() : nullptr;
}
otpParam* otpInstance::getParamByName(const char *name)
{
    for (auto& p : m_params) {
        if (strcmp(p->getName(), name) == 0) {
            return p.get();
        }
    }
    return nullptr;
}

int otpInstance::getNumPorts() const
{
    return (int)m_ports.size();
}
otpPort* otpInstance::getPort(int i)
{
    return i < m_ports.size() ? m_ports[i].get() : nullptr;
}
otpPort* otpInstance::getPortByName(const char *name)
{
    for (auto& p : m_ports) {
        if (strcmp(p->getName(), name) == 0) {
            return p.get();
        }
    }
    return nullptr;
}

void* otpInstance::getUserData() const { return m_userdata; }
void otpInstance::setUsertData(void *v) { m_userdata = v; }
double otpInstance::getFrame() const { return m_frame; }

ImageRGBAu8* otpInstance::getDstImage() { return m_dst_image.get(); }

void otpInstance::beginRender(int width, int height)
{
    m_base_width = width;
    m_base_height = height;
    m_dst_image->resize(m_base_width, m_base_height);

    double hw = double(width) / 2.0;
    double hh = double(height) / 2.0;
    m_rs = {
        { 1, 0 },
        this,
        { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0 },
        1.0,
        60.0, 60.0,
        0.0,
        32,
        INT_MAX,
        3,
        0,
        0,
        0,
        0,
        0,
        { -hw, -hh, hw, hh },
        &m_canceled
    };
    m_probe->handler->start_render(this);
}

bool otpInstance::render(double frame)
{
    if (m_base_width == 0 || m_base_height == 0) {
        utjDebugLog("dst size is zero");
        return false;
    }
    m_dst_image->resize(m_base_width, m_base_height);
    m_frame = frame;
    m_canceled = 0;

    m_probe->handler->do_get_bbox(this, &m_rs, frame, &m_rect);
    size_t mem = m_probe->handler->get_memory_requirement(this, &m_rs, frame, &m_rect);

    m_probe->handler->on_new_frame(this, &m_rs, frame);
    m_probe->handler->do_compute(this, &m_rs, frame, m_dst_image.get());
    m_probe->handler->on_end_frame(this, &m_rs, frame);

    return true;
}

void otpInstance::endRender()
{
    m_probe->handler->end_render(this);
}


