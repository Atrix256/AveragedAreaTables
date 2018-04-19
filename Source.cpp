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

void AATBoxBlur(const std::vector<uint32>& AAT, int width, int height, int radius, const char* baseFileName, bool isStochastic, int scale)
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

			uint32 A = (startX >= 0 && startY >= 0) ? AAT[startY*width + startX] * uint32((startY + 1)*(startX + 1)) / uint32(scale) : 0;
			uint32 B = (startY >= 0) ? AAT[startY*width + endX] * uint32((startY + 1)*(endX + 1)) / uint32(scale) : 0;
			uint32 C = (startX >= 0) ? AAT[endY*width + startX] * uint32((endY + 1)*(startX + 1)) / uint32(scale) : 0;
			uint32 D = AAT[endY*width + endX] * uint32((endY + 1)*(endX + 1)) / uint32(scale);

			uint32 integratedValue = A + D - B - C;

			double size = double((endY - startY)*(endX - startX));

			uint8 average = uint8(0.5f + double(integratedValue) / size);

			result[iy*width + ix] = average;
		}
	}

	char append[32];
	if (isStochastic)
	{
		if(scale > 1)
			sprintf_s(append, "_%i_SAAT_%ix", radius, scale);
		else
			sprintf_s(append, "_%i_SAAT", radius);
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
    std::vector<uint32> AAT, SAAT, AAT4x, AAT16x, SAAT4x, SAAT16x;
	AAT.resize(width*height);
	SAAT.resize(width*height);
	AAT4x.resize(width*height);
	AAT16x.resize(width*height);
	SAAT4x.resize(width*height);
	SAAT16x.resize(width*height);
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_real_distribution<float> dist(0, 1.0f);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
            double value = double(SAT[iy*width + ix]);
			double rangeSize = double((ix + 1)*(iy + 1));
			AAT[iy*width + ix] = uint32(0.5f + (value / rangeSize));  // 0 to 255 since the source is too
			SAAT[iy*width + ix] = uint32(dist(rng) + (value / rangeSize)); // 0 to 255 since the source is too

			AAT4x[iy*width + ix] = uint32(0.5f + 4.0f * (value / rangeSize));  // an extra 2 bits of precision
			AAT16x[iy*width + ix] = uint32(0.5f + 16.0f * (value / rangeSize)); // an extra 4 bits of precision
			SAAT4x[iy*width + ix] = uint32(dist(rng) + 4.0f * (value / rangeSize)); // an extra 2 bits of precision and dithering
			SAAT16x[iy*width + ix] = uint32(dist(rng) + 16.0f * (value / rangeSize)); // an extra 4 bits of precision and dithering
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
		AATBoxBlur(AAT, width, height, radiuses[index], baseFileName, false, 1);

		// box blur with stochastic AAT
		AATBoxBlur(SAAT, width, height, radiuses[index], baseFileName, true, 1);

		// box blur with scaled AAT
		AATBoxBlur(AAT4x, width, height, radiuses[index], baseFileName, false, 4);
		AATBoxBlur(AAT16x, width, height, radiuses[index], baseFileName, false, 16);

		// box blur with stochastic scaled AAT
		AATBoxBlur(SAAT4x, width, height, radiuses[index], baseFileName, true, 4);
		AATBoxBlur(SAAT16x, width, height, radiuses[index], baseFileName, true, 16);
	}
}

int main(int argc, char** argv)
{
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
    
    return 0;
}

/*
TODO:

* blue noise dither instead of white noise dither the stochastic aat?

* also try the thing with adding bits for specifix sized filters. show it breaking down. maybe a filter of 7x7 and a filter of 9x9, and add 6 more bits (handles 8x8 max)
* show breakdown of float as well? esp f16 if you can. http://half.sourceforge.net/
* analyze visually (spit out images?), as well as numerically?

*/