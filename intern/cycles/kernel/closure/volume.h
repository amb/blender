/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#ifndef __VOLUME_H__
#define __VOLUME_H__

CCL_NAMESPACE_BEGIN

/* HENYEY-GREENSTEIN CLOSURE */

/* Given cosine between rays, return probability density that photon bounce to that direction
 * g parameter controls how far it difference from uniform sphere. g=0 uniform diffusion-like, g=1 - very close to sharp single ray. */

ccl_device float single_peaked_henyey_greenstein(float cos_theta, float m_g)
{
	float p = (1.0f - m_g * m_g) / pow(1.0f + m_g * m_g - 2.0f * m_g * cos_theta, 1.5f) / 4.0f / M_PI_F;

	return p;
};

ccl_device int volume_henyey_greenstein_setup(ShaderClosure *sc)
{
	sc->type = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

// just return bsdf at input vector
ccl_device float3 volume_henyey_greenstein_eval_phase(const ShaderClosure *sc, const float3 I, float3 omega_in, float *pdf)
{
	float m_g = sc->data1;
	const float magic_eps = 0.001f;

//	WARNING! I point in backward direction!
//	float cos_theta = dot(I, omega_in);
	float cos_theta = dot(-I, omega_in);

	if(fabsf(m_g) <  magic_eps)
		*pdf = M_1_PI_F * 0.25f; // ?? double check it
	else
		*pdf = single_peaked_henyey_greenstein(cos_theta, m_g);

	return make_float3(*pdf, *pdf, *pdf);
}

ccl_device int volume_henyey_greenstein_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv,
	float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_g = sc->data1;
	const float magic_eps = 0.001f;

	// WARNING! I point in backward direction!

	if(fabsf(m_g) <  magic_eps) {
		*omega_in = sample_uniform_sphere(randu, randv);
		*pdf = M_1_PI_F * 0.25f; // ?? double check it
	}
	else {
		float cos_phi, sin_phi, cos_theta;

		if(fabsf(m_g) <  magic_eps)
			cos_theta = (1.0f - 2.0f * randu);
		else {
			float k = (1.0f - m_g * m_g) / (1.0f - m_g + 2.0f * m_g * randu);
			cos_theta = (1.0f + m_g * m_g - k * k) / (2.0f * m_g);
		//	float cos_theta = 1.0f / (2.0f * m_g) * (1.0f + m_g * m_g - k*k);
		//	float cos_theta = (1.0f - 2.0f * randu);
		//	float cos_theta = randu;
		}
		float sin_theta = sqrt(1 - cos_theta * cos_theta);
		
		float3 T, B;
		make_orthonormals(-I, &T, &B);
		float phi = M_2PI_F * randv;
		cos_phi = cosf(phi);
		sin_phi = sinf(phi);
		*omega_in = sin_theta * cos_phi * T + sin_theta * sin_phi * B + cos_theta * (-I);
		*pdf = single_peaked_henyey_greenstein(cos_theta, m_g);
	}

	*eval = make_float3(*pdf, *pdf, *pdf); // perfect importance sampling
#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = (2 * dot(Ng, dIdx)) * Ng - dIdx;
		*domega_in_dy = (2 * dot(Ng, dIdy)) * Ng - dIdy;
		*domega_in_dx *= 125.0f;
		*domega_in_dy *= 125.0f;
#endif
	return LABEL_REFLECT|LABEL_DIFFUSE;
}

/* TRANSPARENT VOLUME CLOSURE */

ccl_device int volume_transparent_setup(ShaderClosure *sc)
{
	sc->type = CLOSURE_VOLUME_TRANSPARENT_ID;

	return SD_VOLUME;
}

ccl_device float3 volume_transparent_eval_phase(const ShaderClosure *sc, const float3 I, float3 omega_in, float *pdf)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

ccl_device int volume_transparent_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv,
	float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	/* XXX Implement */
	return LABEL_REFLECT|LABEL_DIFFUSE;
}

/* VOLUME CLOSURE */

ccl_device float3 volume_eval_phase(KernelGlobals *kg, const ShaderClosure *sc, const float3 I, float3 omega_in, float *pdf)
{
	float3 eval;

	switch(sc->type) {
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
			eval = volume_henyey_greenstein_eval_phase(sc, I, omega_in, pdf);
			break;
		case CLOSURE_VOLUME_TRANSPARENT_ID:
			eval = volume_transparent_eval_phase(sc, I, omega_in, pdf);
			break;
		default:
			eval = make_float3(0.0f, 0.0f, 0.0f);
			break;
	}

	return eval;
}

ccl_device int volume_sample(KernelGlobals *kg, const ShaderData *sd, const ShaderClosure *sc, float randu,
	float randv, float3 *eval, float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;

	switch(sc->type) {
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
			label = volume_henyey_greenstein_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv, eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_VOLUME_TRANSPARENT_ID:
			label = volume_transparent_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv, eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		default:
			*eval = make_float3(0.0f, 0.0f, 0.0f);
			label = LABEL_NONE;
			break;
	}

//	*eval *= sd->svm_closure_weight;

	return label;
}

CCL_NAMESPACE_END

#endif
