#ifndef _INCLUDE_STV_PATCHMNGR_H_
#define _INCLUDE_STV_PATCHMNGR_H_

#include <map>
#include <vector>

inline std::vector<uint8_t> ByteVectorFromString(const char* s)
{
	std::vector<uint8_t> payload;
	std::string str(s);

	for (size_t i = 0; i < str.length(); ++i) {
		if (str[i] == '\\' && i + 1 < str.length() && str[i + 1] == 'x') {
			if (i + 3 < str.length() && std::isxdigit(str[i + 2]) && std::isxdigit(str[i + 3])) {
				std::string byteString = str.substr(i + 2, 2);
				char* endPtr;
				long byteValue = strtol(byteString.c_str(), &endPtr, 16);

				if (endPtr != byteString.c_str() && *endPtr == '\0' && byteValue >= 0 && byteValue <= 255) {
					payload.push_back(static_cast<uint8_t>(byteValue));
				} else {
					smutils->LogError(myself, "Warning: Invalid byte string '\\x%s' ignored.", byteString.c_str());
				}
				i += 3;
			} else {
				smutils->LogError(myself, "Warning: Incomplete byte sequence '\\x' ignored.");
				i++;
			}
		}
	}

	return payload;
}

struct CPatchParams
{
	const char* m_pszPatchName;
	const char* m_pszSignatureName;
	const char* m_iPatchOffsetName;
	const char* m_pszCheckBytesName;
	const char* m_pszPatchBytesName;

	bool m_bSuccessMsg;
	bool m_bActionMsg;
	bool m_bEnable;

	CPatchParams()
	{
		m_pszPatchName = "Unknown";
		m_pszSignatureName = NULL;
		m_iPatchOffsetName = NULL;
		m_pszCheckBytesName = NULL;
		m_pszPatchBytesName = NULL;

		m_bActionMsg = true;
		m_bSuccessMsg = true;
		m_bEnable = false;
	}
};

class CPatch
{
public:
	const char* m_pszPatchName;
	bool m_bPatchEnable;

	void* m_pSignature;

	int m_iPatchOffset;

	patch_t m_checkBytes;
	patch_t m_originalBytes;
	patch_t m_patchBytes;

	bool m_bActionMsg;

	CPatch()
	{
		m_pszPatchName = NULL;
		m_bPatchEnable = false;

		m_pSignature = NULL;

		m_iPatchOffset = -1;

		m_checkBytes.patch[0] = '\0';
		m_checkBytes.bytes = 0;

		m_originalBytes.patch[0] = '\0';
		m_originalBytes.bytes = 0;

		m_patchBytes.patch[0] = '\0';
		m_patchBytes.bytes = 0;

		m_bActionMsg = true;
	}
};

class CSTVPatch
{
	friend class CPatchMngr;

public:
	inline bool IsPatchChecked()
	{
		return m_bPatchAddrChecked;
	}

	bool PatchEnable()
	{
		if (m_bPatchEnabled) {
			if (m_bActionMsg) {
				Msg("Patch '%s' is already activated!""\n", m_pszPatchName);
			}

			return false;
		}

		if (!m_bPatchAddrChecked) {
			Msg("Patch '%s' has not yet been verified and cannot be activated!""\n", m_pszPatchName);

			return false;
		}

		ApplyPatch((void*)m_patchedAddr, m_iPatchOffset, &m_patchBytes, &m_originalBytes);
		ProtectMemory((void*)m_patchedAddr, m_patchBytes.bytes, PAGE_EXECUTE_READ);

		m_bPatchEnabled = true;

		if (m_bActionMsg) {
			Msg("Patch '%s' activated!""\n", m_pszPatchName);
		}

		return true;
	}

	bool PatchDisable()
	{
		if (!m_bPatchEnabled) {
			if (m_bActionMsg) {
				Msg("Patch '%s' is already deactivated!""\n", m_pszPatchName);
			}

			return false;
		}

		if (!m_bPatchAddrChecked) {
			Msg("Patch '%s' has not yet been verified and cannot be deactivated!""\n", m_pszPatchName);

			return false;
		}

		ApplyPatch((void*)m_patchedAddr, m_iPatchOffset, &m_originalBytes, NULL);
		ProtectMemory((void*)m_patchedAddr, m_originalBytes.bytes, PAGE_EXECUTE_READ);

		m_bPatchEnabled = false;

		if (m_bActionMsg) {
			Msg("Patch '%s' deactivated!""\n", m_pszPatchName);
		}

		return true;
	}

	inline bool TogglePatch(bool bEnable)
	{
		return (bEnable) ? PatchEnable() : PatchDisable();
	}

private:
	const char* m_pszPatchName;

	void* m_patchedAddr;

	unsigned int m_iPatchOffset;

	patch_t m_patchBytes;
	patch_t m_originalBytes;

	bool m_bPatchEnabled;
	bool m_bPatchAddrChecked;
	bool m_bActionMsg;

private:
	CSTVPatch(CPatch* pParams)
	{
		m_pszPatchName = pParams->m_pszPatchName;
		m_patchedAddr = pParams->m_pSignature;
		m_iPatchOffset = pParams->m_iPatchOffset;

		std::memcpy(m_patchBytes.patch, pParams->m_patchBytes.patch, sizeof(pParams->m_patchBytes.patch));
		m_patchBytes.bytes = strlen(reinterpret_cast<const char*>(pParams->m_patchBytes.patch));

		if (m_patchBytes.bytes <= 0) {
			Msg("Error: Patch %s size is zero!""\n", m_pszPatchName);
		}

		m_originalBytes.patch[0] = 0;
		m_originalBytes.bytes = 0;

		m_bPatchEnabled = false;
		m_bActionMsg = pParams->m_bActionMsg;
		m_bPatchAddrChecked = true;
	}

	CSTVPatch() = delete;
	CSTVPatch(const CSTVPatch&) = delete;
	const CSTVPatch& operator=(CSTVPatch&) = delete;
};

class CPatchMngr
{
public:
	static CPatchMngr& GetInstance() {
		static CPatchMngr instance;
		return instance;
	}

	CSTVPatch* InitPatch(CPatchParams* pParams, IGameConfig* gc, char* error, size_t maxlength)
	{
		if (pParams->m_iPatchOffsetName == NULL || strlen(pParams->m_iPatchOffsetName) < 1) {
			ke::SafeSprintf(error, maxlength, "Offset name not set for patch \"%s\"! Please contact the author!""\n", pParams->m_pszPatchName);
			return NULL;
		}

		if (pParams->m_pszSignatureName == NULL || strlen(pParams->m_pszSignatureName) < 1) {
			ke::SafeSprintf(error, maxlength, "Signature name not set for patch \"%s\"! Please contact the author!""\n", pParams->m_pszPatchName);
			return NULL;
		}

		CPatch stvPatch;
		const char* signName = gc->GetKeyValue(pParams->m_pszSignatureName);
		if (signName == NULL) {
			ke::SafeSprintf(error, maxlength, "Unable to find patch section for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", pParams->m_pszSignatureName);

			return false;
		}

		if (!gc->GetMemSig(signName, &stvPatch.m_pSignature)) {
			ke::SafeSprintf(error, maxlength, "Signature \"%s\" not found for patch \"%s\", from game config (file: \"" GAMEDATA_FILE ".txt\")", pParams->m_pszSignatureName, pParams->m_pszPatchName);

			return NULL;
		}

		if (stvPatch.m_pSignature == NULL) {
			ke::SafeSprintf(error, maxlength, "Sigscan for \"%s\" failed for patch \"%s\" (game config file: \"" GAMEDATA_FILE ".txt\")", pParams->m_pszSignatureName, pParams->m_pszPatchName);

			return NULL;
		}

		const char* offsetName = gc->GetKeyValue(pParams->m_iPatchOffsetName);
		if (offsetName == NULL) {
			ke::SafeSprintf(error, maxlength, "Unable to find patch section for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", pParams->m_iPatchOffsetName);

			return false;
		}

		if (!gc->GetOffset(offsetName, &stvPatch.m_iPatchOffset) || stvPatch.m_iPatchOffset < 1) {
			ke::SafeSprintf(error, maxlength, "Unable to get offset for \"%s\" for patch \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", pParams->m_iPatchOffsetName, pParams->m_pszPatchName);

			return NULL;
		}

		static const struct {
			const char* keyBytes;
			patch_t& patch_info;
		} s_patchesBytes[] = {
			{ pParams->m_pszCheckBytesName, stvPatch.m_checkBytes },
			{ pParams->m_pszPatchBytesName, stvPatch.m_patchBytes },
		};

		for (auto&& el : s_patchesBytes) {
			const char* keyValue = gc->GetKeyValue(el.keyBytes);
			if (keyValue == NULL) {
				ke::SafeSprintf(error, maxlength, "Unable to find patch section for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.keyBytes);

				return false;
			}

			std::vector<uint8_t> vecBytes = ByteVectorFromString(keyValue);
			if (vecBytes.size() == 0) {
				ke::SafeSprintf(error, maxlength, "Unable to parse patch section for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.keyBytes);

				return false;
			}

			std::copy(vecBytes.begin(), vecBytes.end(), el.patch_info.patch);
			el.patch_info.bytes = vecBytes.size();
		}

		for (size_t i = 0; i < sizeof(stvPatch.m_checkBytes.patch) && i < stvPatch.m_checkBytes.bytes; i++) {
			byte cCheckByte = *(reinterpret_cast<byte*>((uintptr_t)stvPatch.m_pSignature + stvPatch.m_iPatchOffset + i));

			if (cCheckByte == stvPatch.m_checkBytes.patch[i]) {
				//Msg("[Success] Offset \"%d\" for patch \"%s\", patch size \"%d\". Byte \"%x\" expected, received byte \"%x\".""\n", \
					//stvPatch.m_iPatchOffset, pParams->m_iPatchOffsetName, stvPatch.m_checkBytes.bytes, stvPatch.m_checkBytes.patch[i], cCheckByte);
				continue;
			}

			ke::SafeSprintf(error, maxlength, "Wrong offset \"%d\" for patch \"%s\", patch size \"%d\". Byte \"%x\" expected, received byte \"%x\". Please contact the author!", \
				stvPatch.m_iPatchOffset, pParams->m_iPatchOffsetName, stvPatch.m_checkBytes.bytes, stvPatch.m_checkBytes.patch[i], cCheckByte);

			return NULL;
		}

		stvPatch.m_bActionMsg = pParams->m_bActionMsg;
		stvPatch.m_pszPatchName = pParams->m_pszPatchName;

		CSTVPatch* vspPatch = new CSTVPatch(&stvPatch);
		m_patchesArray[pParams->m_pszPatchName] = vspPatch;

		if (pParams->m_bEnable) {
			vspPatch->PatchEnable();

			if (pParams->m_bSuccessMsg) {
				Msg("Patch '%s' was successfully verified and applied! Sig ptr \"%x\"!""\n", pParams->m_pszPatchName, stvPatch.m_pSignature);
			}

			return vspPatch;
		}

		if (pParams->m_bSuccessMsg) {
			Msg("Patch '%s' was successfully verified. Sig ptr \"%x\"!""\n", pParams->m_pszPatchName, stvPatch.m_pSignature);
		}

		return vspPatch;
	}

	void UnpatchAll()
	{
		for (std::pair<const std::string, CSTVPatch*>& item : m_patchesArray) {
			if (item.second == NULL) {
				continue;
			}

			item.second->PatchDisable();

			delete item.second;
			item.second = NULL;

			Msg("Patch destroy: %s""\n", item.first.c_str());
		}

		m_patchesArray.clear();
	}

private:
	CPatchMngr() {};
	CPatchMngr(const CPatchMngr&) = delete;
	const CPatchMngr& operator=(CPatchMngr&) = delete;

public:
	std::map<std::string, CSTVPatch*> m_patchesArray;
};

#endif // _INCLUDE_STV_PATCHMNGR_H_