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
	std::vector<uint8> result;
	result.resize(width * height);

	float filterSize = float(radius * 2 + 1)*float(radius * 2 + 1);

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
            float average = AverageOfRectangle(source, width, height, ix - radius, iy - radius, ix + radius, iy + radius);
            result[iy*width + ix] = uint8(0.5f + average);
		}
	}

	char append[32];
	sprintf_s(append, "_%i", radius);
	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void SATBoxBlur(const std::vector<uint32>& SAT, int width, int height, int radius, const char* baseFileName)
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

			uint32 A = (startX >= 0 && startY >= 0) ? SAT[startY*width + startX] : 0;
			uint32 B = (startY >= 0) ? SAT[startY*width + endX] : 0;
			uint32 C = (startX >= 0) ? SAT[endY*width + startX] : 0;
			uint32 D = SAT[endY*width + endX];

			uint32 integratedValue = A + D - B - C;

			double size = double((endY - startY)*(endX - startX));

			uint8 average = uint8(0.5f + double(integratedValue) / size);

			result[iy*width + ix] = average;
		}
	}

	char append[32];
	sprintf_s(append, "_%i_SAT", radius);
	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void AATBoxBlur(const std::vector<uint32>& AAT, int width, int height, int radius, const char* baseFileName, bool isStochastic, int scale, bool isBlue)
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

			float A = float((startX >= 0 && startY >= 0) ? AAT[startY*width + startX] * uint32((startY + 1)*(startX + 1)) / uint32(scale) : 0);
			float B = float((startY >= 0) ? AAT[startY*width + endX] * uint32((startY + 1)*(endX + 1)) / uint32(scale) : 0);
			float C = float((startX >= 0) ? AAT[endY*width + startX] * uint32((endY + 1)*(startX + 1)) / uint32(scale) : 0);
			float D = float(AAT[endY*width + endX] * uint32((endY + 1)*(endX + 1)) / uint32(scale));

			float integratedValue = A + D - B - C;

			float size = float((endY - startY)*(endX - startX));

			uint8 average = uint8(0.5f + integratedValue / size);

			result[iy*width + ix] = average;
		}
	}

	char append[64];
	if (isStochastic)
	{
		if (isBlue)
		{
			if (scale > 1)
				sprintf_s(append, "_%i_SAAT_Blue_%ix", radius, scale);
			else
				sprintf_s(append, "_%i_SAAT_Blue", radius);
		}
		else
		{
			if (scale > 1)
				sprintf_s(append, "_%i_SAAT_White_%ix", radius, scale);
			else
				sprintf_s(append, "_%i_SAAT_White", radius);
		}
	}
	else
	{
		if (scale > 1)
			sprintf_s(append, "_%i_AAT_%ix", radius, scale);
		else
			sprintf_s(append, "_%i_AAT", radius);
	}

	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void TestAATvsSAT(uint8* source, int width, int height, const char* baseFileName)
{
    // make SAT
    std::vector<uint32> SAT;
    SAT.resize(width * height);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
			uint32 i_xy = uint32(source[iy*width + ix]);

			uint32 I_xny = (ix > 0) ? SAT[iy*width + (ix - 1)] : 0;
			uint32 I_xyn = (iy > 0) ? SAT[(iy - 1)*width + ix] : 0;
			uint32 I_xnyn = (ix > 0 && iy > 0) ? SAT[(iy - 1)*width + (ix - 1)] : 0;

			SAT[iy*width + ix] = i_xy + I_xny + I_xyn - I_xnyn;
        }
    }

    // make AAT, stochastic AAT, and scaled AATs
    std::vector<uint32> AAT, SAAT, SAATBlue, AAT4x, AAT16x, AAT256x, SAATBlue4x, SAATBlue16x, SAAT4x, SAAT16x;
	AAT.resize(width*height);
	SAAT.resize(width*height);
	SAATBlue.resize(width*height);
	AAT4x.resize(width*height);
	AAT16x.resize(width*height);
	AAT256x.resize(width*height);
	SAAT4x.resize(width*height);
	SAAT16x.resize(width*height);
	SAATBlue4x.resize(width*height);
	SAATBlue16x.resize(width*height);
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_real_distribution<float> dist(0, 1.0f);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
			// tile the blue noise texture across the image to get blue noise random numbers per pixel. blue noise tiles well.
			float blueNoise = float(g_blueNoisePixels[((iy%g_blueNoiseHeight) * g_blueNoiseWidth + (ix%g_blueNoiseWidth))*g_blueNoiseChannels])/255.0f;

            double value = double(SAT[iy*width + ix]);
			double rangeSize = double((ix + 1)*(iy + 1));
			AAT[iy*width + ix] = uint32(0.5f + (value / rangeSize));  // 0 to 255 since the source is too
			SAAT[iy*width + ix] = uint32(dist(rng) + (value / rangeSize)); // 0 to 255 since the source is too. + white noise dithering

			SAATBlue[iy*width + ix] = uint32(blueNoise + (value / rangeSize)); // 0 to 255 since the source is too. + blue noise dithering

			AAT4x[iy*width + ix] = uint32(0.5f + 4.0f * (value / rangeSize));  // an extra 2 bits of precision (10 bit unorm)
			AAT16x[iy*width + ix] = uint32(0.5f + 16.0f * (value / rangeSize)); // an extra 4 bits of precision (12 bit unorm)
			AAT256x[iy*width + ix] = uint32(0.5f + 256.0f * (value / rangeSize)); // an extra 8 bits of precision (16 bit unorm)

			SAAT4x[iy*width + ix] = uint32(dist(rng) + 4.0f * (value / rangeSize)); // an extra 2 bits of precision and white noise dithering
			SAAT16x[iy*width + ix] = uint32(dist(rng) + 16.0f * (value / rangeSize)); // an extra 4 bits of precision and white noise dithering

			SAATBlue4x[iy*width + ix] = uint32(blueNoise + 4.0f * (value / rangeSize)); // an extra 2 bits of precision and blue noise dithering
			SAATBlue16x[iy*width + ix] = uint32(blueNoise + 16.0f * (value / rangeSize)); // an extra 4 bits of precision and blue noise dithering
        }
    }

	int radiuses[] = { 0, 1, 5, 25, 100 };

	for (size_t index = 0; index < _countof(radiuses); ++index)
	{
		// regular box blur
		BoxBlur(source, width, height, radiuses[index], baseFileName);

		// box blur with SAT
		SATBoxBlur(SAT, width, height, radiuses[index], baseFileName);

		// box blur with AAT
		AATBoxBlur(AAT, width, height, radiuses[index], baseFileName, false, 1, false);

		// box blur with stochastic AAT
		AATBoxBlur(SAAT, width, height, radiuses[index], baseFileName, true, 1, false);
		AATBoxBlur(SAATBlue, width, height, radiuses[index], baseFileName, true, 1, true);

		// box blur with scaled AAT
		AATBoxBlur(AAT4x, width, height, radiuses[index], baseFileName, false, 4, false);
		AATBoxBlur(AAT16x, width, height, radiuses[index], baseFileName, false, 16, false);
		AATBoxBlur(AAT256x, width, height, radiuses[index], baseFileName, false, 256, false);

		// box blur with stochastic scaled AAT
		AATBoxBlur(SAAT4x, width, height, radiuses[index], baseFileName, true, 4, false);
		AATBoxBlur(SAAT16x, width, height, radiuses[index], baseFileName, true, 16, false);
		AATBoxBlur(SAATBlue4x, width, height, radiuses[index], baseFileName, true, 4, true);
		AATBoxBlur(SAATBlue16x, width, height, radiuses[index], baseFileName, true, 16, true);
	}
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

	// random number test
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

	stbi_image_free(g_blueNoisePixels);
    
    return 0;
}

/*
TODO:

* make a save function that makes a 4 channel image (copy 1 channel to rgb) and make alpha be solid for everything except lower right pixel. - twitter encoding! (:

* also try the thing with adding bits for specifix sized filters. show it breaking down. maybe a filter of 7x7 and a filter of 9x9, and add 6 more bits (handles 8x8 max)
* show breakdown of float as well? esp f16 if you can. http://half.sourceforge.net/

* analyze visually (spit out images?), as well as numerically if there is anything that seems worth doing that?

* does scaled AAT need to work in float internally to prevent uint32 overflow? i think so... 16 bit unorm having issues
 * yes, and needs to convert from 0 to 255 to 0 to 1 i think...
 ? do the other things need to work in float too?

* Peter-Pike's tweet? https://twitter.com/PeterPikeSloan/status/986850728374185985

* sebastien aaltonen was curious about 16 bit unorm
 * share when not having overflow problems!

*/