// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// minkar-vkdraw's triangle: one color, from a push constant.
#version 450

layout(push_constant) uniform Color { vec4 color; } pc;
layout(location = 0) out vec4 outColor;

void main()
{
	outColor = pc.color;
}
