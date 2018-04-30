#include <stdio.h>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int32_t int32;

int g_blueNoiseWidth, g_blueNoiseHeight, g_blueNoiseChannels;
stbi_uc* g_blueNoisePixels = nullptr;

template <typename T>
float AverageOfRectangle(T* data, int width, int height, int sx, int sy, int ex, int ey)
{
    sx = std::min(std::max(sx, 0), width - 1);
    ex = std::min(std::max(ex, 0), width - 1);
    sy = std::min(std::max(sy, 0), height - 1);
    ey = std::min(std::max(ey, 0), height - 1);

    float sum = 0.0f;
    for (int iy = sy; iy <= ey; ++iy)
    {
        for (int ix = sx; ix <= ex; ++ix)
        {
            sum += float(data[iy*width+ix]);
        }
    }

    float sampleCount = float(ey - sy + 1)*float(ex - sx + 1);

    return sum / sampleCount;
}

void BoxBlur(const uint8* source, int width, int height, int radius, const char* baseFileName)
{
	std::vector<uint8> resultPing;
    std::vector<uint8> resultPong;
	resultPing.resize(width * height);
    resultPong.resize(width * height);

    float filterSize = float(radius * 2 + 1);

    // horizontal blur from source to ping
    for (int iy = 0; iy < height; ++iy)
    {
        for (int ix = 0; ix < width; ++ix)
        {
            float average = AverageOfRectangle(source, width, height, ix - radius, iy, ix + radius, iy);
            resultPing[iy*width + ix] = uint8(0.5f + average);
        }
    }

    // vertical blur from ping to pong
    for (int iy = 0; iy < height; ++iy)
    {
        for (int ix = 0; ix < width; ++ix)
        {
            float average = AverageOfRectangle(&resultPing[0], width, height, ix, iy - radius, ix, iy + radius);
            resultPong[iy*width + ix] = uint8(0.5f + average);
        }
    }

    char append[32];
    sprintf_s(append, "_%i", radius);
    char fileName[256];
    sprintf_s(fileName, baseFileName, append);
    printf("%s\n", fileName);
    stbi_write_png(fileName, width, height, 1, &resultPong[0], width);
}

void SATBoxBlurBiased(const std::vector<int32>& SAT, int width, int height, int radius, const char* baseFileName, const char* technique, int bias)
{
	std::vector<uint8> result;
	result.resize(SAT.size());

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
			int startX = std::max(ix - radius - 1, -1);
			int startY = std::max(iy - radius - 1, -1);

			int endX = std::min(ix + radius, width - 1);
			int endY = std::min(iy + radius, height - 1);

			int32 A = (startX >= 0 && startY >= 0) ? SAT[startY*width + startX] : bias;
			int32 B = (startY >= 0) ? SAT[startY*width + endX] : -bias;
			int32 C = (startX >= 0) ? SAT[endY*width + startX] : -bias;
			int32 D = SAT[endY*width + endX];

			int32 integratedValue = (A + D - B - C);

			double size = double((endY - startY)*(endX - startX));

			uint8 average = uint8(float(bias) + 0.5f + double(integratedValue) / size);

			result[iy*width + ix] = average;
		}
	}

    char append[64];
    sprintf_s(append, "_%i_%s", radius, technique);

	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
    printf("%s\n", fileName);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void SATBoxBlur(const std::vector<uint32>& SAT, int width, int height, int radius, const char* baseFileName, const char* technique, int scale, int numBits)
{
	std::vector<uint8> result;
	result.resize(SAT.size());

	uint32 maxValue = numBits == 32 ? uint32(-1) : uint32(1 << numBits) - 1;

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
			if (ix > 3 && iy > 3)
			{
				int ijkl = 0;
				(void)ijkl;
			}

			int startX = std::max(ix - radius - 1, -1);
			int startY = std::max(iy - radius - 1, -1);

			int endX = std::min(ix + radius, width - 1);
			int endY = std::min(iy + radius, height - 1);

			uint32 A = (startX >= 0 && startY >= 0) ? SAT[startY*width + startX] : 0;
			uint32 B = (startY >= 0) ? SAT[startY*width + endX] : 0;
			uint32 C = (startX >= 0) ? SAT[endY*width + startX] : 0;
			uint32 D = SAT[endY*width + endX];

			A &= maxValue;
			B &= maxValue;
			C &= maxValue;
			D &= maxValue;

			// TODO: should we do it as float? i feel like "yes" because shaders will do that, but i don't think the wrap around works there? dunno...
#if 0
			// Note that double can perfectly represent any uint32, but it's a float when a shader gets it
			float fA = float(double(A) / double(maxValue));
			float fB = float(double(B) / double(maxValue));
			float fC = float(double(C) / double(maxValue));
			float fD = float(double(D) / double(maxValue));

			fA *= float(scale);
			fB *= float(scale);
			fC *= float(scale);
			fD *= float(scale);

			float integratedValue = fA + fD - fB - fC;

			float size = float((endY - startY)*(endX - startX));

			uint8 average = uint8(0.5 + double(maxValue) * double(integratedValue / size));
#else
			uint32 integratedValue = A + D - B - C;
			integratedValue *= scale;
			integratedValue &= maxValue;

			float size = float((endY - startY)*(endX - startX));

			uint8 average = uint8(0.5 + double(integratedValue) / double(size));
#endif

			result[iy*width + ix] = average;
		}
	}

    char append[64];
    sprintf_s(append, "_%i_%s_%ix", radius, technique, scale);

	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
    printf("%s\n", fileName);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void AATBoxBlur(const std::vector<uint32>& AAT, int width, int height, int radius, const char* baseFileName, const char* technique, int scale)
{
	std::vector<uint8> result;
	result.resize(AAT.size());

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
			int startX = std::max(ix - radius - 1, -1);
			int startY = std::max(iy - radius - 1, -1);

			int endX = std::min(ix + radius, width - 1);
			int endY = std::min(iy + radius, height - 1);

            // This function aims to mimic unorm and shader behaviors.
            // * Scale implicitly describes the number of bits of storage above 8. (eg 10 bit would have a scale of 4)
            // * It converts to float because that's what shaders work in.
            // * It multiplies by area after converting to float because that's when shaders would be able to do their work to turn an average back into an area.

			float A = float((startX >= 0 && startY >= 0) ? AAT[startY*width + startX] : 0) / float(256 * scale);
            A *= float((startY + 1)*(startX + 1));

            float B = float((startY >= 0) ? AAT[startY*width + endX] : 0) / float(256 * scale);
            B *= float((startY + 1)*(endX + 1));

            float C = float((startX >= 0) ? AAT[endY*width + startX] : 0) / float(256 * scale);
            C *= float((endY + 1)*(startX + 1));

            float D = float(AAT[endY*width + endX]) / float(256 * scale);
            D *= float((endY + 1)*(endX + 1));

			float integratedValue = A + D - B - C;

			float size = float((endY - startY)*(endX - startX));

			uint8 average = uint8(0.5f + 255.0f * integratedValue / size);

			result[iy*width + ix] = average;
		}
	}

	char append[64];
    sprintf_s(append, "_%i_%s_%ix", radius, technique, scale);

	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
    printf("%s\n", fileName);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void TestAATvsSAT(uint8* source, int width, int height, const char* baseFileName)
{
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> dist(0, 1.0f);

    // make Summed Area Tables
    std::vector<uint32> SAT;
	std::vector<int32> SATBiased127;
    SAT.resize(width * height);
	SATBiased127.resize(width * height);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
			// make SAT
			{
				uint32 i_xy = uint32(source[iy*width + ix]);

				uint32 I_xny = (ix > 0) ? SAT[iy*width + (ix - 1)] : 0;
				uint32 I_xyn = (iy > 0) ? SAT[(iy - 1)*width + ix] : 0;
				uint32 I_xnyn = (ix > 0 && iy > 0) ? SAT[(iy - 1)*width + (ix - 1)] : 0;

				SAT[iy*width + ix] = i_xy + I_xny + I_xyn - I_xnyn;
			}

			// make biased SAT
			{
				int32 i_xy = int32(source[iy*width + ix]) - 127;

				int32 I_xny = (ix > 0) ? SATBiased127[iy*width + (ix - 1)] : - 127;
				int32 I_xyn = (iy > 0) ? SATBiased127[(iy - 1)*width + ix] : - 127;
				int32 I_xnyn = (ix > 0 && iy > 0) ? SATBiased127[(iy - 1)*width + (ix - 1)] : - 127;

				SATBiased127[iy*width + ix] = i_xy + I_xny + I_xyn - I_xnyn;
			}
        }
    }

	// get and write out max / min value in biased SAT
	{
		int32 SATBiased127Min = SATBiased127[0];
		int32 SATBiased127Max = SATBiased127[0];
		for (int32 v : SATBiased127)
		{
			SATBiased127Min = std::min(SATBiased127Min, v);
			SATBiased127Max = std::max(SATBiased127Max, v);
		}
		uint32 SATMax = 0;
		for (uint32 v : SAT)
			SATMax = std::max(SATMax, v);
		char fileName[256];
		sprintf_s(fileName, baseFileName, "");
		strcat_s(fileName, ".txt");
		
		FILE* file = nullptr;
		fopen_s(&file, fileName, "w+t");
		fprintf(file, "SAT Max: %u (%i bits)\n", SATMax, int(std::ceilf(std::log2f(float(SATMax)))));
		fprintf(file, "Biased 127 Min = %i (%i bits)\n", SATBiased127Min, int(std::ceilf(1.0f + std::log2f(std::fabsf(float(SATBiased127Min))))));
		fprintf(file, "Biased 127 Max = %i (%i bits)\n", SATBiased127Max, int(std::ceilf(1.0f + std::log2f(std::fabsf(float(SATBiased127Max))))));
		fclose(file);
	}

    // make Averaged Area Tables (AATs) and other Summed Area Table variants
    std::vector<uint32> AAT, AAT4x, AAT16x, AAT256x;
	std::vector<uint32> AATBlue, AATBlue4x, AATBlue16x, AATBlue256x;
	std::vector<uint32> AATWhite, AATWhite4x, AATWhite16x, AATWhite256x;
    std::vector<uint32> SAT4x, SAT16x, SAT256x;
    std::vector<uint32> SATBlue4x, SATBlue16x, SATBlue256x;
    std::vector<uint32> SATWhite4x, SATWhite16x, SATWhite256x;
	AAT.resize(width*height);
	AATWhite.resize(width*height);
	AATBlue.resize(width*height);
	AAT4x.resize(width*height);
	AAT16x.resize(width*height);
	AAT256x.resize(width*height);
	AATWhite4x.resize(width*height);
	AATWhite16x.resize(width*height);
    AATWhite256x.resize(width*height);
	AATBlue4x.resize(width*height);
	AATBlue16x.resize(width*height);
    AATBlue256x.resize(width*height);
    SAT4x.resize(width * height);
    SAT16x.resize(width * height);
    SAT256x.resize(width * height);
    SATWhite4x.resize(width * height);
    SATWhite16x.resize(width * height);
    SATWhite256x.resize(width * height);
    SATBlue4x.resize(width * height);
    SATBlue16x.resize(width * height);
    SATBlue256x.resize(width * height);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
			// tile the blue noise texture across the image to get blue noise random numbers per pixel. blue noise tiles well.
			float blueNoise = float(g_blueNoisePixels[((iy%g_blueNoiseHeight) * g_blueNoiseWidth + (ix%g_blueNoiseWidth))*g_blueNoiseChannels])/255.0f;
            float whiteNoise = dist(rng);

            double value = double(SAT[iy*width + ix]);
			double rangeSize = double((ix + 1)*(iy + 1));

            // ------------------ AAT's ------------------

            // rounding
			AAT[iy*width + ix] = uint32(0.5f + (value / rangeSize)); 
			AAT4x[iy*width + ix] = uint32(0.5f + 4.0f * (value / rangeSize));  // an extra 2 bits of precision (10 bit unorm)
			AAT16x[iy*width + ix] = uint32(0.5f + 16.0f * (value / rangeSize)); // an extra 4 bits of precision (12 bit unorm)
			AAT256x[iy*width + ix] = uint32(0.5f + 256.0f * (value / rangeSize)); // an extra 8 bits of precision (16 bit unorm)

            // white noise dithering
            AATWhite[iy*width + ix] = uint32(whiteNoise + (value / rangeSize));
			AATWhite4x[iy*width + ix] = uint32(whiteNoise + 4.0f * (value / rangeSize)); // an extra 2 bits of precision and white noise dithering
			AATWhite16x[iy*width + ix] = uint32(whiteNoise + 16.0f * (value / rangeSize)); // an extra 4 bits of precision and white noise dithering
            AATWhite256x[iy*width + ix] = uint32(whiteNoise + 256.0f * (value / rangeSize)); // an extra 8 bits of precision and white noise dithering

            // blue noise dithering
            AATBlue[iy*width + ix] = uint32(blueNoise + (value / rangeSize));
			AATBlue4x[iy*width + ix] = uint32(blueNoise + 4.0f * (value / rangeSize)); // an extra 2 bits of precision and blue noise dithering
			AATBlue16x[iy*width + ix] = uint32(blueNoise + 16.0f * (value / rangeSize)); // an extra 4 bits of precision and blue noise dithering
            AATBlue256x[iy*width + ix] = uint32(blueNoise + 256.0f * (value / rangeSize)); // an extra 8 bits of precision and blue noise dithering

            // ------------------ SAT's ------------------

            // NOTE: doubles can exactly represent all uint32 integers

            // rounding 
            SAT4x[iy*width + ix] = uint32(0.5 + double(SAT[iy*width + ix]) / 4.0);
            SAT16x[iy*width + ix] = uint32(0.5 + double(SAT[iy*width + ix]) / 16.0);
            SAT256x[iy*width + ix] = uint32(0.5 + double(SAT[iy*width + ix]) / 256.0);

            // white noise dithering 
            SATWhite4x[iy*width + ix] = uint32(double(whiteNoise) + double(SAT[iy*width + ix]) / 4.0);
            SATWhite16x[iy*width + ix] = uint32(double(whiteNoise) + double(SAT[iy*width + ix]) / 16.0);
            SATWhite256x[iy*width + ix] = uint32(double(whiteNoise) + double(SAT[iy*width + ix]) / 256.0);

            // blue noise dithering 
            SATBlue4x[iy*width + ix] = uint32(double(blueNoise) + double(SAT[iy*width + ix]) / 4.0);
            SATBlue16x[iy*width + ix] = uint32(double(blueNoise) + double(SAT[iy*width + ix]) / 16.0);
            SATBlue256x[iy*width + ix] = uint32(double(blueNoise) + double(SAT[iy*width + ix]) / 256.0);
        }
    }

	int radiuses[] = { 0, 1, 5, 25, 100 };

	for (size_t index = 0; index < _countof(radiuses); ++index)
	{
		// regular box blur of source image
		BoxBlur(source, width, height, radiuses[index], baseFileName);

		// box blur with biased SAT
		SATBoxBlurBiased(SATBiased127, width, height, radiuses[index], baseFileName, "SATBiased127", 127);

		// box blur with rounded SAT
		SATBoxBlur(SAT, width, height, radiuses[index], baseFileName, "SAT", 1, 32);
        SATBoxBlur(SAT4x, width, height, radiuses[index], baseFileName, "SAT", 4, 32);
        SATBoxBlur(SAT16x, width, height, radiuses[index], baseFileName, "SAT", 16, 32);
        SATBoxBlur(SAT256x, width, height, radiuses[index], baseFileName, "SAT", 256, 32);

        // box blur with white noise stochastically rounded SAT
        SATBoxBlur(SATWhite4x, width, height, radiuses[index], baseFileName, "SATWhite", 4, 32);
        SATBoxBlur(SATWhite16x, width, height, radiuses[index], baseFileName, "SATWhite", 16, 32);
        SATBoxBlur(SATWhite256x, width, height, radiuses[index], baseFileName, "SATWhite", 256, 32);

        // box blur with blue noise stochastically rounded SAT
        SATBoxBlur(SATBlue4x, width, height, radiuses[index], baseFileName, "SATBlue", 4, 32);
        SATBoxBlur(SATBlue16x, width, height, radiuses[index], baseFileName, "SATBlue", 16, 32);
        SATBoxBlur(SATBlue256x, width, height, radiuses[index], baseFileName, "SATBlue", 256, 32);

		// box blur with rounded AAT
        AATBoxBlur(AAT, width, height, radiuses[index], baseFileName, "AAT", 1);
		AATBoxBlur(AAT4x, width, height, radiuses[index], baseFileName, "AAT", 4);
		AATBoxBlur(AAT16x, width, height, radiuses[index], baseFileName, "AAT", 16);
		AATBoxBlur(AAT256x, width, height, radiuses[index], baseFileName, "AAT", 256);

		// box blur with white noise stochastically rounded AAT
        AATBoxBlur(AATWhite, width, height, radiuses[index], baseFileName, "AATWhite", 1);
		AATBoxBlur(AATWhite4x, width, height, radiuses[index], baseFileName, "AATWhite", 4);
		AATBoxBlur(AATWhite16x, width, height, radiuses[index], baseFileName, "AATWhite", 16);
        AATBoxBlur(AATWhite256x, width, height, radiuses[index], baseFileName, "AATWhite", 256);

        // box blur with blue noise stochastically rounded AAT
        AATBoxBlur(AATBlue, width, height, radiuses[index], baseFileName, "AATBlue", 1);
		AATBoxBlur(AATBlue4x, width, height, radiuses[index], baseFileName, "AATBlue", 4);
		AATBoxBlur(AATBlue16x, width, height, radiuses[index], baseFileName, "AATBlue", 16);
        AATBoxBlur(AATBlue256x, width, height, radiuses[index], baseFileName, "AATBlue", 256);
	}

	// do a 7x7 and a 9x9 box blur with the 14 bit SAT. 7x7 should be fine. 9x9 should not be.
	SATBoxBlur(SAT, width, height, 1, baseFileName, "SAT14bit", 1, 14);
	SATBoxBlur(SAT, width, height, 2, baseFileName, "SAT14bit", 1, 14);
	SATBoxBlur(SAT, width, height, 3, baseFileName, "SAT14bit", 1, 14);
	SATBoxBlur(SAT, width, height, 4, baseFileName, "SAT14bit", 1, 14);
}

int main(int argc, char** argv)
{
	g_blueNoisePixels = stbi_load("bluenoise.png", &g_blueNoiseWidth, &g_blueNoiseHeight, &g_blueNoiseChannels, 4);

	// image test
	{
		int width, height, components;
		stbi_uc* pixels = stbi_load("scenery.png", &width, &height, &components, 1);
		TestAATvsSAT(pixels, width, height, "out/scenery%s.png");
		stbi_image_free(pixels);
	}

	// TODO: activate this again
	// random number test
	/*
	{
		std::random_device rd;
		std::mt19937 rng(rd());
		std::uniform_int_distribution<unsigned int> dist(0, 255);

		std::vector<uint8> source;
		source.resize(1024*1024);
		for (uint8& v : source)
			v = dist(rng);

		TestAATvsSAT(&source[0], 1024, 1024, "out/rng%s.png");
	}
	*/

	stbi_image_free(g_blueNoisePixels);
    
    return 0;
}

/*
TODO:

* i think you should make functions to do conversions: UNORMToFloat and floatToUNORM

* sat biased doesn't look right in pixel row / column 0. see why? maybe you aren't adding bias anymore...

* try the thing with adding bits for specific sized filters and allowing overflow. show it breaking down. maybe a filter of 7x7 and a filter of 9x9, and add 6 more bits (handles 8x8 max)
 * I did, but it's not looking correct

* that thing about a low res SAT (bilinearly interpolated) with a high res one giving offsets
 * make sure you understand what things need to be what sizes and bit depths




* show breakdown of float as well? esp f16 if you can. http://half.sourceforge.net/

* analyze visually (spit out images?), as well as numerically if there is anything that seems worth doing that? could also do a scaled(?) image diff.

? it seems like we may still be getting overflow, even though we are working with floats. investigate before making blog post w/ final images etc.
 * esp on the 16 bit unorm that sebastien was curious about?

? make SATBoxBlur work in floats too, to be consistent with AAT / shaders stuff... but it's like a u32 unorm when not applying scale... probably need to convert via double.




 * sebastien aaltonen was curious about 16 bit unorm
  * share when not having overflow problems!


 * Maybe topics for another post:
 * Peter-Pike's tweet? https://twitter.com/PeterPikeSloan/status/986850728374185985
  * idea is something like have a lower resolution SAT, with higher resolution SAT describing the details missing. Like a bilinear data fit, and then the missing details in the higher resolution details.
  * if doing mips, could have each mip describe data only to amount of detail needed. each higher res mip adds higher frequencies not missed by the previous mip. 
 * could do tiling testing: https://twitter.com/JonOlick/status/986980257164152832



 Blog notes:
 * scaled SAT's to lose low end bits instead of high.
  * If you know a minimum filter size, this can use fewer bits
  * but, if you can get away with this, you really should just shrink your source texture.
* If you know maximum size, you can allow rollover for sizes above that.
 * these 2 things let you squeeze the bits a bit.


 links:

 SAT and IBL: http://www.cs.unc.edu/techreports/06-017.pdf


*/