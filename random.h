#pragma once

#include "types.h"

#define RND_U32 uint32_t
#define RND_U64 uint64_t
#include "rnd.h"

namespace random
{
	template <
		typename T,
		typename Base,
		void (*SeedFunc)(T*, Base),
		Base (*NextFunc)(T*),
		float (*NextFloatFunc)(T*),
		int (*RangeFunc)(T*, int, int)>
	class RandomGen
	{
	public:
		explicit RandomGen(Base seed)
		{
			SeedFunc(&rng, seed);
		}

		Base Next() { return NextFunc(&rng); }
		float NextF(float max = 1.0f) { return NextFloatFunc(&rng) * max; }
		int Range(int min, int max) { return RangeFunc(&rng, min, max); }
		float RangeF(float min, float max) { return NextF(max - min) + min; }

	private:
		T rng;
	};

	using PcgGen = RandomGen<rnd_pcg_t, RND_U32, rnd_pcg_seed, rnd_pcg_next, rnd_pcg_nextf, rnd_pcg_range>;
	using WellGen = RandomGen<rnd_well_t, RND_U32, rnd_well_seed, rnd_well_next, rnd_well_nextf, rnd_well_range>;
	using GameRandGen = RandomGen<rnd_gamerand_t, RND_U32, rnd_gamerand_seed, rnd_gamerand_next, rnd_gamerand_nextf, rnd_gamerand_range>;
	using XorShiftGen = RandomGen<rnd_xorshift_t, RND_U64, rnd_xorshift_seed, rnd_xorshift_next, rnd_xorshift_nextf, rnd_xorshift_range>;
}