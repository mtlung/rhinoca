#include "pch.h"
#include "roGraphicsTestBase.win.h"
#include "../../roar/render/roCanvas.h"
#include "../../roar/render/roTexture.h"

using namespace ro;

namespace ro {

extern Resource* resourceCreateBmp(const char*, ResourceManager*);
extern Resource* resourceCreateJpeg(const char*, ResourceManager*);
extern bool resourceLoadBmp(Resource*, ResourceManager*);
extern bool resourceLoadJpeg(Resource*, ResourceManager*);

}

struct CanvasTest : public GraphicsTestBase
{
	CanvasTest()
	{
		resourceManager.addFactory(resourceCreateBmp, resourceLoadBmp);
		resourceManager.addFactory(resourceCreateJpeg, resourceLoadJpeg);
	}

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
	canvas.init(context);

	// Init texture
	TexturePtr texture = resourceManager.loadAs<Texture>("EdSplash.jpg");
	CHECK(texture);

	while(texture && keepRun()) {
		driver->clearColor(0, 0, 0, 0);
		canvas.beginDraw();
		canvas.drawImage(texture->handle, 10, 50, 100, 100);
		canvas.endDraw();
		driver->swapBuffers();
	}

	texture = NULL;
	resourceManager.shutdown();
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
	canvas.init(context);

	// Initialize our canvas which use it's own texture as render target
	Canvas auxCanvas;
	auxCanvas.init(context);
	auxCanvas.initTargetTexture(800, 600);

	// Init texture
	TexturePtr texture = resourceManager.loadAs<Texture>("EdSplash.bmp");

	while(keepRun())
	{
		// Draw to auxCanvas
		auxCanvas.beginDraw();
		auxCanvas.save();

		driver->clearStencil(0);
		auxCanvas.clearRect(0, 0, 800, 600);

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
		auxCanvas.endDraw();

//		auxCanvas.drawImage(texture->handle, 0, 0, 100, 100);

		// Draw auxCanvas to the main canvas
		canvas.beginDraw();
//		driver->clearColor(0.05f, 0.05f, 0.05f, 1);
		driver->clearColor(1, 1, 1, 1);
		canvas.setGlobalAlpha(0.99f);
		canvas.drawImage(auxCanvas.targetTexture->handle, 0, 0);
		canvas.endDraw();

		driver->swapBuffers();
	}

	texture = NULL;
	resourceManager.shutdown();
}
