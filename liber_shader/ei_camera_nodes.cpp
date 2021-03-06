/**************************************************************************
 * Copyright (C) 2013 Elvic Liang<len3dev@gmail.com>
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#define EI_OSL_INTEROP
#include <ei_shaderx.h>
#include <OSL/dual_vec.h>

inline void concentric_sample_disk(
	eiScalar & dx, eiScalar & dy, 
	eiScalar u1, eiScalar u2)
{
	eiScalar r, theta;
	eiScalar sx = 2.0f * u1 - 1.0f;
	eiScalar sy = 2.0f * u2 - 1.0f;
	if (sx == 0.0f && sy == 0.0f) {
		dx = 0.0f;
		dy = 0.0f;
		return;
	}
	if (sx >= -sy) {
		if (sx > sy) {
			r = sx;
			if (sy > 0.0f)
				theta = sy/r;
			else
				theta = 8.0f + sy/r;
		}
		else {
			r = sy;
			theta = 2.0f - sx/r;
		}
	}
	else {
		if (sx <= sy) {
			r = -sx;
			theta = 4.0f - sy/r;
		}
		else {
			r = -sy;
			theta = 6.0f + sx/r;
		}
	}
	theta *= (eiScalar)EI_PI / 4.0f;
	dx = r * cosf(theta);
	dy = r * sinf(theta);
}

inline eiVector uniform_sample_triangle(eiScalar u1, eiScalar u2)
{
	eiVector bary;
	
	bary[0] = 1.0f - sqrtf(u1);
	bary[1] = u2 * sqrtf(u1);
	bary[2] = 1.0f - bary[0] - bary[1];
	
	return bary;
}

inline void uniform_sample_poly_disk(
	eiScalar & dx, eiScalar & dy, 
	eiScalar u1, eiScalar u2, 
	eiInt blades, 
	eiScalar rotation)
{
	eiInt count = blades;
	if (count < 3)
	{
		count = 3;
	}
	
	eiInt i = (eiInt)((eiScalar)count * u1 * 0.99999f);
	u1 = (u1 * (eiScalar)count - (eiScalar)i);
	
	eiScalar t0 = (eiScalar)i / (eiScalar)count * (2.0f * (eiScalar)EI_PI);
	eiScalar t1 = (eiScalar)(i + 1) / (eiScalar)count * (2.0f * (eiScalar)EI_PI);
	
	eiScalar x0 = cosf(t0 + rotation);
	eiScalar y0 = sinf(t0 + rotation);
	eiScalar x1 = cosf(t1 + rotation);
	eiScalar y1 = sinf(t1 + rotation);
	
	eiVector bary = uniform_sample_triangle(u1, u2);
	
	dx = x0 * bary[0] + x1 * bary[1];
	dy = y0 * bary[0] + y1 * bary[1];
}

/** Standard perspective/orthographic camera 
 * with depth of field support
 */
lens (dof_camera)

	enum
	{
		e_fstop = 0, 
		e_fplane, 
		e_blades, 
		e_rotation, 
	};

	static void parameters()
	{
		declare_scalar(fstop, 1.0f);
		declare_scalar(fplane, 1.0f);
		declare_int(blades, 6);
		declare_scalar(rotation, 0.0f);
	}

	static void init()
	{
	}

	static void exit()
	{
	}

	void init_node()
	{
	}

	void exit_node()
	{
	}

	eiBool support(
		eiNode *cam, 
		eiInt feature, 
		void *feature_params)
	{
		return ei_std_camera_support(cam, feature, feature_params);
	}

	eiBool object_to_screen(
		eiNode *cam, 
		eiVector *rpos, 
		const eiVector *opos, 
		const eiMatrix *object_to_view)
	{
		return ei_std_camera_object_to_screen(cam, rpos, opos, object_to_view);
	}

	void update_world_bbox(
		eiNode *cam, 
		const eiBBox *world_bbox)
	{
		ei_std_camera_update_world_bbox(cam, world_bbox);
	}

	eiBool generate_ray(
		eiNode *cam, 
		eiCameraOutput *out)
	{
		eiBool ret_val = ei_std_camera_generate_ray(this, cam, out);

		/* modify ray generated by standard camera to simulate DOF */
		eiScalar focal = ei_node_get_scalar(cam, EI_CAMERA_focal);
		if (focal != EI_MAX_SCALAR) /* only work for perspective camera */
		{
			eiScalar fstop = eval_scalar(fstop);
			eiScalar fplane = eval_scalar(fplane);
			eiInt blades = eval_int(blades);
			eiScalar rotation = eval_scalar(rotation);

			eiVector2 rand;
			eiScalar dof_dx, dof_dy;
			ei_lens_sample(&rand, this);

			if (blades < 3)
			{
				concentric_sample_disk(
					dof_dx, dof_dy, 
					rand.x, rand.y);
			}
			else
			{
				uniform_sample_poly_disk(
					dof_dx, dof_dy, 
					rand.x, rand.y, 
					blades, 
					rotation);
			}

			out->E = ei_point(
				dof_dx * fstop * 0.5f, 
				dof_dy * fstop * 0.5f, 
				0.0f);

			out->I = normalize(out->I * (fplane / absf(out->I[2])) - out->E);
		}

		ei_ray_from_camera(out, cam, time);
		return ret_val;
	}

end_lens (dof_camera)

/** Spherical projection camera
 */
lens (spherical_camera)

	enum
	{
		e_stereo = 0, 
		e_eye_distance, 
	};

	static void parameters()
	{
		declare_bool(stereo, EI_TRUE);
		declare_scalar(eye_distance, 5.0f);
	}

	static void init()
	{
	}

	static void exit()
	{
	}

	void init_node()
	{
	}

	void exit_node()
	{
	}

	eiBool support(
		eiNode *cam, 
		eiInt feature, 
		void *feature_params)
	{
		if (feature == EI_FEATURE_MULTI_VIEW_RENDER)
		{
			eiScalar res_x = (eiScalar)ei_node_get_int(cam, EI_CAMERA_res_x);
			eiBool stereo = eval_bool(stereo);
			eiVector2 *image_subdiv = (eiVector2 *)feature_params;

			if (stereo)
			{
				image_subdiv->x = 2.0f / res_x;
			}

			return EI_TRUE;
		}

		return EI_TRUE;
	}

	eiBool object_to_screen(
		eiNode *cam, 
		eiVector *rpos, 
		const eiVector *opos, 
		const eiMatrix *object_to_view)
	{
		/* transform from object space to view space */
		eiVector v_pos = point_transform(*opos, *object_to_view);

		/* prevent from invalid projection */
		if (almost_zero(v_pos, EI_SCALAR_EPS))
		{
			return EI_FALSE;
		}

		/* get input parameters */
		eiScalar res_x = (eiScalar)ei_node_get_int(cam, EI_CAMERA_res_x);
		eiScalar res_y = (eiScalar)ei_node_get_int(cam, EI_CAMERA_res_y);

		eiBool stereo = eval_bool(stereo);
		if (stereo)
		{
			res_x *= 0.5f;
		}

		/* project from view space to screen space */
		rpos->z = normalize_len(v_pos, v_pos);
		eiScalar theta = atan2f(v_pos.x, - v_pos.z);
		eiScalar phi = asinf(v_pos.y);
		rpos->x = (theta * (0.5f / (eiScalar)EI_PI) + 0.5f) * res_x;
		rpos->y = (1.0f - (phi * (0.5f / (eiScalar)EI_PI_2) + 0.5f)) * res_y;
		
		return EI_TRUE;
	}

	void update_world_bbox(
		eiNode *cam, 
		const eiBBox *world_bbox)
	{
	}

	eiBool generate_ray(
		eiNode *cam, 
		eiCameraOutput *out)
	{
		/* get input parameters */
		eiInt res_x = ei_node_get_int(cam, EI_CAMERA_res_x);
		eiInt res_y = ei_node_get_int(cam, EI_CAMERA_res_y);
		eiScalar hither = ei_node_get_scalar(cam, EI_CAMERA_clip_hither);
		eiScalar yon = ei_node_get_scalar(cam, EI_CAMERA_clip_yon);
		eiVector2 raster = raster_pos();
		eiScalar sx = clamp((raster.x / (eiScalar)res_x) * 2.0f - 1.0f, -1.0f, 1.0f);
		eiScalar sy = clamp((1.0f - raster.y / (eiScalar)res_y) * 2.0f - 1.0f, -1.0f, 1.0f);
		eiScalar dsx = 2.0f / (eiScalar)res_x;
		eiScalar dsy = 2.0f / (eiScalar)res_y;

		/* set output origin */
		out->E = 0.0f;
		out->dEdx = 0.0f;
		out->dEdy = 0.0f;

		eiBool stereo = eval_bool(stereo);
		if (stereo)
		{
			eiScalar eye_distance = eval_scalar(eye_distance);
			if (sx < 0.0f)
			{
				raster.x = raster.x * 2.0f + 1.0f;
				out->E.x -= 0.5f * eye_distance;
			}
			else
			{
				raster.x = raster.x * 2.0f - 1.0f;
				out->E.x += 0.5f * eye_distance;
			}
			dsx *= 2.0f;
		}

		/* compute spherical projection */
		eiScalar theta = (eiScalar)EI_PI * sx;
		eiScalar phi = (eiScalar)EI_PI_2 * sy;
		eiScalar sin_theta = sinf(theta);
		eiScalar cos_theta = cosf(theta);
		eiScalar sin_phi = sinf(phi);
		eiScalar cos_phi = cosf(phi);
		/* set output direction */
		out->I.x = sin_theta * cos_phi;
		out->I.y = sin_phi;
		out->I.z = -cos_theta * cos_phi;
		/* compute derivative with respect to raster X */
		out->dIdx.x = cos_theta;
		out->dIdx.y = 0.0f;
		out->dIdx.z = sin_theta;
		out->dIdx *= ((eiScalar)EI_PI * cos_phi * dsx);
		/* compute derivative with respect to raster Y */
		out->dIdy.x = - sin_theta * sin_phi;
		out->dIdy.y = cos_phi;
		out->dIdy.z = cos_theta * sin_phi;
		out->dIdy *= ((eiScalar)EI_PI_2 * dsy);
		
		/* set near and far clipping */
		out->t_near = max(hither, EI_SCALAR_EPS);
		out->t_far = min(yon, EI_BIG_SCALAR);

		/* transform result into world space */
		ei_ray_from_camera(out, cam, time);
		return EI_TRUE;
	}

end_lens (spherical_camera)

/** Cubemap projection camera
 */
lens (cubemap_camera)

	enum
	{
		e_stereo = 0, 
		e_eye_distance, 
	};

	static void parameters()
	{
		declare_bool(stereo, EI_TRUE);
		declare_scalar(eye_distance, 5.0f);
	}

	static void init()
	{
	}

	static void exit()
	{
	}

	void init_node()
	{
	}

	void exit_node()
	{
	}

	eiBool support(
		eiNode *cam, 
		eiInt feature, 
		void *feature_params)
	{
		if (feature == EI_FEATURE_MULTI_VIEW_RENDER)
		{
			eiScalar res_x = (eiScalar)ei_node_get_int(cam, EI_CAMERA_res_x);
			eiBool stereo = eval_bool(stereo);
			eiVector2 *image_subdiv = (eiVector2 *)feature_params;

			if (stereo)
			{
				image_subdiv->x = 12.0f / res_x;
			}
			else
			{
				image_subdiv->x = 6.0f / res_x;
			}

			return EI_TRUE;
		}

		return EI_TRUE;
	}

	eiBool object_to_screen(
		eiNode *cam, 
		eiVector *rpos, 
		const eiVector *opos, 
		const eiMatrix *object_to_view)
	{
		/* transform from object space to view space */
		eiVector v_pos = point_transform(*opos, *object_to_view);

		/* prevent from invalid projection */
		if (almost_zero(v_pos, EI_SCALAR_EPS))
		{
			return EI_FALSE;
		}

		/* get input parameters */
		eiScalar res_x = (eiScalar)ei_node_get_int(cam, EI_CAMERA_res_x);
		eiScalar res_y = (eiScalar)ei_node_get_int(cam, EI_CAMERA_res_y);
		eiScalar focal = 1.0f;
		eiScalar image_center_y = res_y * 0.5f;
		eiScalar camera_to_pixel_y = - res_y / 2.0f;

		eiBool stereo = eval_bool(stereo);
		if (stereo)
		{
			res_x *= 0.5f;
		}

		eiVector abs_v_pos = ei_vector(absf(v_pos.x), absf(v_pos.y), absf(v_pos.z));
		eiInt axis = abs_v_pos.max_axis();
		eiVector s_pos;
		eiScalar z, p_pos_x;

		switch (axis)
		{
		case X_AXIS:
			{
				if (v_pos.x > 0.0f) /* +X */
				{
					s_pos.x = v_pos.z;
					s_pos.y = v_pos.y;
					s_pos.z = - v_pos.x;

					z = focal / s_pos.z;
					p_pos_x = res_x * ((0.5f - s_pos.x * z * 0.5f) / 6.0f);
				}
				else /* -X */
				{
					s_pos.x = - v_pos.z;
					s_pos.y = v_pos.y;
					s_pos.z = v_pos.x;

					z = focal / s_pos.z;
					p_pos_x = res_x * (((0.5f - s_pos.x * z * 0.5f) + 1.0f) / 6.0f);
				}
			}
			break;

		case Y_AXIS:
			{
				if (v_pos.y > 0.0f) /* +Y */
				{
					s_pos.x = - v_pos.x;
					s_pos.y = - v_pos.z;
					s_pos.z = - v_pos.y;

					z = focal / s_pos.z;
					p_pos_x = res_x * (((0.5f - s_pos.x * z * 0.5f) + 2.0f) / 6.0f);
				}
				else /* -Y */
				{
					s_pos.x = v_pos.x;
					s_pos.y = - v_pos.z;
					s_pos.z = v_pos.y;

					z = focal / s_pos.z;
					p_pos_x = res_x * (((0.5f - s_pos.x * z * 0.5f) + 3.0f) / 6.0f);
				}
			}
			break;

		default: /* Z_AXIS */
			{
				if (v_pos.z > 0.0f) /* +Z */
				{
					s_pos.x = - v_pos.x;
					s_pos.y = v_pos.y;
					s_pos.z = - v_pos.z;

					z = focal / s_pos.z;
					p_pos_x = res_x * (((0.5f - s_pos.x * z * 0.5f) + 5.0f) / 6.0f);
				}
				else /* -Z */
				{
					s_pos.x = v_pos.x;
					s_pos.y = v_pos.y;
					s_pos.z = v_pos.z;

					z = focal / s_pos.z;
					p_pos_x = res_x * (((0.5f - s_pos.x * z * 0.5f) + 4.0f) / 6.0f);
				}
			}
			break;
		}

		/* project from view space to screen space */
		if (p_pos_x < 0.0f || p_pos_x > res_x)
		{
			return EI_FALSE;
		}

		eiScalar p_pos_y = image_center_y - s_pos.y * z * camera_to_pixel_y;

		if (p_pos_y < 0.0f || p_pos_y > res_y)
		{
			return EI_FALSE;
		}

		rpos->x = p_pos_x;
		rpos->y = p_pos_y;
		rpos->z = z;

		return EI_TRUE;
	}

	void update_world_bbox(
		eiNode *cam, 
		const eiBBox *world_bbox)
	{
	}

	eiBool generate_ray(
		eiNode *cam, 
		eiCameraOutput *out)
	{
		/* get input parameters */
		eiInt res_x = ei_node_get_int(cam, EI_CAMERA_res_x);
		eiInt res_y = ei_node_get_int(cam, EI_CAMERA_res_y);
		eiScalar hither = ei_node_get_scalar(cam, EI_CAMERA_clip_hither);
		eiScalar yon = ei_node_get_scalar(cam, EI_CAMERA_clip_yon);
		eiVector2 raster = raster_pos();

		/* precompute some parameters */
		eiScalar focal = 1.0f;
		eiScalar image_center_y = (eiScalar)res_y * 0.5f;
		eiScalar pixel_to_camera_x = 2.0f / ((eiScalar)res_x / 6.0f);
		eiScalar pixel_to_camera_y = -2.0f / (eiScalar)res_y;

		/* set the output ray origin */
		out->E = 0.0f;
		out->dEdx = 0.0f;
		out->dEdy = 0.0f;

		eiBool stereo = eval_bool(stereo);
		if (stereo)
		{
			eiScalar eye_distance = eval_scalar(eye_distance);
			eiScalar half_res_x = (eiScalar)res_x * 0.5f;
			if (raster.x < half_res_x)
			{
				raster.x *= 2.0f;
				out->E.x -= 0.5f * eye_distance;
			}
			else
			{
				raster.x = (raster.x - half_res_x) * 2.0f;
				out->E.x += 0.5f * eye_distance;
			}
			pixel_to_camera_x *= 2.0f;
		}

		/* set the output ray direction */
		eiScalar sx = (raster.x / (eiScalar)res_x) * 6.0f;
		if (sx < 1.0f) /* +X */
		{
			out->I.x = focal;
			out->I.y = ((raster.y - image_center_y) * pixel_to_camera_y);
			out->I.z = (2.0f * sx - 1.0f);
			out->dIdx = ei_vector(0.0f, 0.0f, pixel_to_camera_x);
			out->dIdy = ei_vector(0.0f, - pixel_to_camera_y, 0.0f);
		}
		else if (sx < 2.0f) /* -X */
		{
			out->I.x = - focal;
			out->I.y = (raster.y - image_center_y) * pixel_to_camera_y;
			out->I.z = - (2.0f * (sx - 1.0f) - 1.0f);
			out->dIdx = ei_vector(0.0f, 0.0f, - pixel_to_camera_x);
			out->dIdy = ei_vector(0.0f, - pixel_to_camera_y, 0.0f);
		}
		else if (sx < 3.0f) /* +Y */
		{
			out->I.x = (2.0f * (sx - 2.0f) - 1.0f);
			out->I.y = focal;
			out->I.z = (raster.y - image_center_y) * pixel_to_camera_y;
			out->dIdx = ei_vector(pixel_to_camera_x, 0.0f, 0.0f);
			out->dIdy = ei_vector(0.0f, 0.0f, - pixel_to_camera_y);
		}
		else if (sx < 4.0f) /* -Y */
		{
			out->I.x = (2.0f * (sx - 3.0f) - 1.0f);
			out->I.y = - focal;
			out->I.z = - ((raster.y - image_center_y) * pixel_to_camera_y);
			out->dIdx = ei_vector(pixel_to_camera_x, 0.0f, 0.0f);
			out->dIdy = ei_vector(0.0f, 0.0f, pixel_to_camera_y);
		}
		else if (sx < 5.0f) /* -Z */
		{
			out->I.x = (2.0f * (sx - 4.0f) - 1.0f);
			out->I.y = (raster.y - image_center_y) * pixel_to_camera_y;
			out->I.z = - focal;
			out->dIdx = ei_vector(pixel_to_camera_x, 0.0f, 0.0f);
			out->dIdy = ei_vector(0.0f, - pixel_to_camera_y, 0.0f);
		}
		else /* +Z */
		{
			out->I.x = - (2.0f * (sx - 5.0f) - 1.0f);
			out->I.y = (raster.y - image_center_y) * pixel_to_camera_y;
			out->I.z = focal;
			out->dIdx = ei_vector(- pixel_to_camera_x, 0.0f, 0.0f);
			out->dIdy = ei_vector(0.0f, - pixel_to_camera_y, 0.0f);
		}
		
		/* normalize the ray direction */
		OSL::Dual2<OSL::Vec3> temp(to_vec3(out->I), to_vec3(out->dIdx), to_vec3(out->dIdy));
		temp = normalize(temp);
		from_vec3(out->I, temp.val());
		from_vec3(out->dIdx, temp.dx());
		from_vec3(out->dIdy, temp.dy());

		/* set near and far clipping */
		out->t_near = max(out->t_near, EI_SCALAR_EPS);
		out->t_far = min(out->t_far, EI_BIG_SCALAR);

		/* transform result into world space */
		ei_ray_from_camera(out, cam, time);
		return EI_TRUE;
	}

end_lens (cubemap_camera)
