#include "lib/render/border_mask.h"

#include "lib/i18n.h"
#include "lib/tga.h"
#include "lib/vfs.h"

namespace ow
{

// Shadery: src/shaders/border_mask.vert/.frag.

BorderMask::~BorderMask()
{
	if (dev_ && tex_) dev_->destroyTexture(tex_);
}

bool BorderMask::init(rhi::Device& dev, const Vfs& vfs, std::string& err)
{
	dev_ = &dev;
	auto data = vfs.read("Data/GUI/bordermask.tga");
	Image img;
	if (!data || !loadTga(*data, img, &err))
	{
		if (err.empty()) err = strf(tr("brak %s"), "Data/GUI/bordermask.tga");
		return false;
	}
	rhi::TextureDesc t;
	t.width = img.width;
	t.height = img.height;
	t.mipmaps = false;
	t.repeatU = t.repeatV = false;
	tex_ = dev.createTexture(t, img.rgba.data());
	rhi::PipelineDesc d;
	d.shader = "border_mask";
	d.strides = {2 * sizeof(float)};
	d.attribs = {{0, 0, 2, 0}};
	d.blend = rhi::Blend::Alpha;
	d.depth = rhi::DepthTest::Off;
	d.depthWrite = false;
	pipe_ = dev.createPipeline(d, err);
	return pipe_ != rhi::kNull && tex_ != rhi::kNull;
}

void BorderMask::draw(int x0, int w, int h)
{
	if (!dev_ || !pipe_ || !tex_) return;
	const float quad[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
	const float size[4] = {float(w), float(h), float(x0), 0.f};
	dev_->setViewport(x0, 0, w, h);
	dev_->bindPipeline(pipe_);
	dev_->setUniforms(rhi::kBindDraw, size, sizeof size);
	dev_->bindTexture(0, tex_);
	dev_->setVertexData(0, quad, sizeof quad);
	dev_->draw(6);
}

} // namespace ow
