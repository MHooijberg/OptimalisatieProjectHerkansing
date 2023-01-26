#pragma once
#include "precomp.h"
namespace Tmpl8
{
	enum class movableType {
		BEAM,
		ROCKET,
		TANK
	};

	class movable
	{

	public:
		movableType moveable_type;
	};

}