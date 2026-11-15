/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ProfileWriteNotice.h"

#include <emscripten.h>

// clang-format off
// A worker thread sends a message to the page (post.js gives it to web/page/launcher-cloud.js). The page's own thread
// calls the page at once.
EM_JS(void, openblack_notice_profile_write, (), {
	if (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope) {
		postMessage({ openblackProfileWrite: true });
	} else if (globalThis.openblackProfileWrite) {
		globalThis.openblackProfileWrite();
	}
});
// clang-format on

void openblack::web::NoticeProfileWrite()
{
	openblack_notice_profile_write();
}
