#ifndef _INCLUDE_PLUGIN_VSP_PATCH_BODY_H_
#define _INCLUDE_PLUGIN_VSP_PATCH_BODY_H_

#include "tools.h"

struct Detour_C_BaseEntity__SetParent_s
{
	struct
	{
		/**
		 * C_BaseEntity::index
		 * xref from "No prediction datamap_t for entity %d/%s", passed as second arg
		 */
		int index = -1;

		/**
		 * C_BaseEntity::m_hNetworkMoveParent
		 * xref from "m_hNetworkMoveParent"
		 */
		int m_hNetworkMoveParent = -1;

		/**
		 * C_BaseEntity::m_pMoveParent
		 * in SetParent
		 */
		int m_pMoveParent = -1;
	} Offsets;

	struct
	{
		/**
		 * C_BaseEntity::SetParent
		 * xref from constructor for C_ViewmodelAttachmentModel, called on instance along the way
		 */
		signature_t SetParent;
	} Signatures;

	bool m_bEnabled{ false };
	char* m_patchedAddr{ nullptr };
	int m_patchedBytes{ -1 };

	bool LoadDetour();
	void UnloadDetour();
};

extern Detour_C_BaseEntity__SetParent_s Detour_C_BaseEntity__SetParent;

void Detour_Trampoline_Main();
void Detour_Trampoline_ToOriginal();

#endif // _INCLUDE_PLUGIN_VSP_PATCH_BODY_H_