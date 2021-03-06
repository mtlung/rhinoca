#include "pch.h"
#include "roGraphicsTestBase.win.h"
#include "../../roar/render/roCanvas.h"
#include "../../roar/render/roFont.h"
#include "../../roar/render/roTexture.h"

using namespace ro;

struct CanvasTest : public GraphicsTestBase
{
	Canvas canvas;
};

static const unsigned driverIndex = 0;

static const char* driverStr[] = 
{
	"GL",
	"DX11"
};

TEST_FIXTURE(CanvasTest, drawImage)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);
	canvas.init();

	// Init texture
	TexturePtr texture = subSystems.resourceMgr->loadAs<Texture>("EdSplash.png");
//	TexturePtr texture = subSystems.resourceMgr->loadAs<Texture>("http://udn.epicgames.com/pub/webbg_udn.jpg");
//	TexturePtr texture = subSystems.resourceMgr->loadAs<Texture>("http://4.bp.blogspot.com/-1rQdRXHNKAM/T4BU4ndq01I/AAAAAAAAAgc/ZgexwVGnk1U/s1600/Cupisz_Robert_Light_Probe_Interpolation2.jpg");
	CHECK(texture);

	while(texture && keepRun()) {
		driver->clearColor(0, 0, 0, 0);
		canvas.drawImage(texture->handle, 10, 50, 100, 100);
		driver->swapBuffers();
	}

	texture = NULL;
}

static void testLineWidth(Canvas& c)
{
	for(float i=0; i<10; ++i) {
		c.setLineWidth(1+i);
		c.beginPath();
		c.moveTo(5+i*14,5);
		c.lineTo(5+i*14,140);
		c.stroke();
	}
}

static void testStrokeStyle(Canvas& c)
{
	for(float i=0; i<6; ++i) for (float j=0; j<6; ++j) {
		c.setStrokeColor(0, (255-42.5f*i)/255, (255-42.5f*j)/255, 1);
		c.beginPath();
		c.arc(12.5f+j*25,12.5f+i*25,10,0,roTWO_PI,true);
		c.stroke();
	}
}

static void testLineCap(Canvas& c)
{
	const char* lineCap[] = { "butt", "round", "square" };

	// Draw guides
	c.setStrokeColor(0, 153.0f/255, 1, 1);
	c.beginPath();
	c.moveTo(10,10);
	c.lineTo(140,10);
	c.moveTo(10,140);
	c.lineTo(140,140);
	c.stroke();

	// Draw lines
	c.setStrokeColor(0, 0, 0, 1);
	for(float i=0;i<roCountof(lineCap);++i) {
		c.setLineWidth(15);
		c.setLineCap(lineCap[int(i)]);
		c.beginPath();
		c.moveTo(25+i*50,10);
		c.lineTo(25+i*50,140);
		c.stroke();
	}		
}

static void testLineJoin(Canvas& c)
{
	const char* lineJoin[] = { "round", "bevel", "miter" };
	c.setLineWidth(10);
	for(float i=0;i<roCountof(lineJoin);++i) {
		c.setLineJoin(lineJoin[int(i)]);
		c.beginPath();
		c.moveTo(-5,5+i*40);
		c.lineTo(35,45+i*40);
		c.lineTo(75,5+i*40);
		c.lineTo(115,45+i*40);
		c.lineTo(155,5+i*40);
		c.stroke();
	}
}

static void testFillStyle(Canvas& c)
{
	for(float i=0;i<6;++i) for(float j=0;j<6;++j) {
		c.setFillColor((255-42.5f*i)/255, (255-42.5f*j)/255, 0, 1);
		c.fillRect(j*25,i*25,25,25);
	}
}

static void testRgba(Canvas& c)
{
	// Draw background
	c.setFillColor(1,221.0f/255,0,1);
	c.fillRect(0,0,150,37.5);
	c.setFillColor(102.0f/255,204.0f/255,0,1);
	c.fillRect(0,37.5,150,37.5);
	c.setFillColor(0,153.0f/255,1,1);
	c.fillRect(0,75,150,37.5);
	c.setFillColor(1,51.0f/255,0,1);
	c.fillRect(0,112.5,150,37.5);

	// Draw semi transparent rectangles
	for(float i=0;i<10;++i) {
		c.setFillColor(1,1,1,(i+1)/10);
		for(float j=0;j<4;++j) {
			c.fillRect(5+i*14,5+j*37.5f,14,27.5f);
		}
	}
}

static void testGlobalAlpha(Canvas& c)
{
	// Draw background
	c.setFillColor(1,221.0f/255,0,1);
	c.fillRect(0,0,75,75);
	c.setFillColor(102.0f/255,204.0f/255,0,1);
	c.fillRect(75,0,75,75);
	c.setFillColor(0,153.0f/255,1,1);
	c.fillRect(0,75,75,75);
	c.setFillColor(1,51.0f/255,0,1);
	c.fillRect(75,75,75,75);
	c.setFillColor(1,1,1,1);

	// Set transparency value
	c.setGlobalAlpha(0.2f);

	// Draw semi transparent circles
	for(float i=0;i<7;++i){
		c.beginPath();
		c.arc(75,75,10+10*i,0,roTWO_PI,true);
		c.fill();
	}

	c.setGlobalAlpha(1);
	c.setFillColor(0, 0, 0, 1);
}

static void testLinearGradient(Canvas& c)
{
	// Create gradients
	void* lingrad = c.createLinearGradient(0,0,0,150);
	c.addGradientColorStop(lingrad, 0, 0,171.0f/255,235.0f/255,1);
	c.addGradientColorStop(lingrad, 0.5f, 1,1,1,1);
	c.addGradientColorStop(lingrad, 0.5f, 102.0f/255,204.0f/255,0,1);
	c.addGradientColorStop(lingrad, 1, 1,1,1,1);

	void* lingrad2 = c.createLinearGradient(0,50,0,95);
	c.addGradientColorStop(lingrad2, 0.5f, 0,0,0,1);
	c.addGradientColorStop(lingrad2, 1, 0,0,0,0);

	// Assign gradients to fill and stroke styles
	c.setFillGradient(lingrad);
	c.setStrokeGradient(lingrad2);

	c.setLineWidth(4);

	// Draw shapes
	c.fillRect(10,10,130,130);
	c.strokeRect(50,50,50,50);

	// Cleanup
	c.destroyGradient(lingrad);
	c.destroyGradient(lingrad2);
}

static void testRadialGradient(Canvas& c)
{
	// Create gradients
	void* radgrad = c.createRadialGradient(45,45,10,52,50,30);
	c.addGradientColorStop(radgrad, 0, 167.0f/255,211.0f/255,12.0f/255,1);
	c.addGradientColorStop(radgrad, 0.9f, 1.0f/255,159.0f/255,98.0f/255,1);
	c.addGradientColorStop(radgrad, 1, 1.0f/255,159.0f/255,98.0f/255,0);

	void* radgrad2 = c.createRadialGradient(105,105,20,112,120,50);
	c.addGradientColorStop(radgrad2, 0, 1,95.0f/255,152.0f/255,1);
	c.addGradientColorStop(radgrad2, 0.75f, 1,1.0f/255,136.0f/255,1);
	c.addGradientColorStop(radgrad2, 1, 1,1.0f/255,136.0f/255,0);

	void* radgrad3 = c.createRadialGradient(95,15,15,102,20,40);
	c.addGradientColorStop(radgrad3, 0, 0,201.0f/255,1,1);
	c.addGradientColorStop(radgrad3, 0.8f, 0,181.0f/255,226.0f/255,1);
	c.addGradientColorStop(radgrad3, 1, 0,201.0f/255,1,0);

	void* radgrad4 = c.createRadialGradient(0,150,50,0,140,90);
	c.addGradientColorStop(radgrad4, 0, 244.0f/255,242.0f/255,1.0f/255,1);
	c.addGradientColorStop(radgrad4, 0.8f, 228.0f/255,199.0f/255,0,1);
	c.addGradientColorStop(radgrad4, 1, 228.0f/255,199.0f/255,0,0);

	// Draw shapes
	c.setFillGradient(radgrad4);
	c.fillRect(0,0,150,150);
	c.setFillGradient(radgrad3);
	c.fillRect(0,0,150,150);
	c.setFillGradient(radgrad2);
	c.fillRect(0,0,150,150);
	c.setFillGradient(radgrad);
	c.fillRect(0,0,150,150);

	// Cleanup
	c.destroyGradient(radgrad);
	c.destroyGradient(radgrad2);
	c.destroyGradient(radgrad3);
	c.destroyGradient(radgrad4);
}

TEST_FIXTURE(CanvasTest, drawToCanvas)
{
	createWindow(800, 600);
	initContext(driverStr[driverIndex]);
	canvas.init();

	// Initialize our canvas which use it's own texture as render target
	Canvas auxCanvas;
	auxCanvas.init();
	auxCanvas.initTargetTexture(context->width, context->height);

	// Init texture
	TexturePtr texture = subSystems.resourceMgr->loadAs<Texture>("EdSplash.png");

	while(keepRun())
	{
		// Draw to auxCanvas
		auxCanvas.save();

		driver->clearStencil(0);
		auxCanvas.clearRect(0, 0, (float)context->width, (float)context->height);

		auxCanvas.setFillColor(0, 0, 0, 1);
		auxCanvas.setStrokeColor(0, 0, 0, 1);

		auxCanvas.translate(20, 20);
		auxCanvas.setLineWidth(1);
		testLineWidth(auxCanvas);

		auxCanvas.setLineWidth(1);
		auxCanvas.translate(180, 0);
		testStrokeStyle(auxCanvas);

		auxCanvas.setLineWidth(1);
		auxCanvas.translate(180, 0);
		testLineCap(auxCanvas);

		auxCanvas.setLineWidth(1);
		auxCanvas.translate(180, 0);
		testLineJoin(auxCanvas);

		auxCanvas.translate(-180 * 3, 180);
		testFillStyle(auxCanvas);

		auxCanvas.translate(180, 0);
		testRgba(auxCanvas);

		auxCanvas.translate(180, 0);
		testGlobalAlpha(auxCanvas);

		auxCanvas.translate(180, 0);
		testLinearGradient(auxCanvas);

		auxCanvas.translate(-180 * 3, 180);
		testRadialGradient(auxCanvas);

		auxCanvas.restore();

//		auxCanvas.drawImage(texture->handle, 0, 0, 100, 100);

		// Draw auxCanvas to the main canvas
		canvas.makeCurrent();
//		driver->clearColor(0.05f, 0.05f, 0.05f, 1);
		driver->clearColor(1, 1, 1, 1);
		canvas.setGlobalAlpha(0.99f);
		canvas.drawImage(auxCanvas.targetTexture->handle, 0, 0);

		driver->swapBuffers();
	}

	texture = NULL;
}

TEST_FIXTURE(CanvasTest, drawPixel)
{
	createWindow(200, 200);
	initContext(driverStr[driverIndex]);
	canvas.init();

	Canvas auxCanvas;
	auxCanvas.init();
	auxCanvas.initTargetTexture(context->width, context->height);

	while(keepRun())
	{
		canvas.clearRect(0, 0, (float)context->width, (float)context->height);

		roSize rowBytes = 0;
		roUint8* pixels = auxCanvas.lockPixelWrite(rowBytes);

		if(!pixels) {
			CHECK(false);
			continue;
		}

		for(roSize i=0; i<auxCanvas.height(); ++i) for(roSize j=0; j<auxCanvas.width(); ++j) {
			const float freq = 3.14159265358979323f / 200;
			roUint8 r = roUint8(128 * sinf(i * freq) + 127);
			roUint8 g = roUint8(128 * sinf(j * freq) + 127);
			roUint8 b = roUint8(128 * cosf(i * freq) + 127);

			const roUint8 color[4] = { r, g, b, 255 };
			roSize y = auxCanvas.targetTexture->handle->isYAxisUp ? auxCanvas.height() - i - 1 : i;
			roSize offset = y * rowBytes + j * 4;
			(int&)(pixels[offset]) = (int&)color;
		}

		auxCanvas.unlockPixelData();

		canvas.drawImage(auxCanvas.targetTexture->handle, 0, 0);

		driver->swapBuffers();
	}
}

#include "../../roar/base/roTextResource.h"

TEST_FIXTURE(CanvasTest, drawText)
{
	createWindow(800, 600);
	initContext(driverStr[driverIndex]);
	canvas.init();

	roUint16 beginChi = 0x4E00;
	roUint16 endChi = 0x9FFF;
	roUint16 codePoint = beginChi;

	TextResourcePtr text = new TextResource("main.cpp");
	text = subSystems.resourceMgr->loadAs<TextResource>(text.get(), resourceLoadText);

	while(keepRun())
	{
		driver->clearStencil(0);
//		driver->clearColor(1, 1, 1, 1);
		canvas.clearRect(0, 0, (float)context->width, (float)context->height);

		canvas.setGlobalColor(1, 1, 1, 1);

		canvas.setFont("italic 10pt / 110% Calibri");
//		canvas.fillText("AT_-Hello world!", 0, 40, 0);
		canvas.fillText(text->data.c_str(), 0, 40, 0);

//		canvas.setFont("Comic Sans MS");
//		canvas.fillText("Lung Man Tat", 0, 60, 0);

//		canvas.setFont("DFKai-SB");
//		canvas.fillText(str.c_str(), 0, 100, 0);

		driver->swapBuffers();
	}
}
