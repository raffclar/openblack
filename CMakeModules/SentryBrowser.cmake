# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
# The Sentry browser SDK of the web game. The configure step gets the bundle of one fixed version, and checks it with
# its SHA-256 hash. The build puts the bundle and its licence next to the page, so the page loads it from the site of
# the game and never from another site at run time.

set(OPENBLACK_SENTRY_BROWSER_VERSION "11.10.0")
set(OPENBLACK_SENTRY_BROWSER_SHA256
    "fbc2ddccc2431b84ba24478cbfd987f75b59c3e4d3102753ad5bb687f28b5210"
)
set(OPENBLACK_SENTRY_BROWSER_LICENSE_SHA256
    "b6d1586282dc00c1c830bd4c6c4a122e8e8c4b898fd42a5f4ee99870374f2d3d"
)
set(OPENBLACK_SENTRY_BROWSER_DIR "${CMAKE_BINARY_DIR}/sentry-browser")
set(OPENBLACK_SENTRY_BROWSER_SCRIPT
    "${OPENBLACK_SENTRY_BROWSER_DIR}/sentry-browser.min.js"
)
set(OPENBLACK_SENTRY_BROWSER_LICENSE
    "${OPENBLACK_SENTRY_BROWSER_DIR}/sentry-browser.LICENSE.txt"
)

# Gets a file once. A file that is there with the correct hash does not download again.
function (openblack_sentry_browser_download url path hash)
  file(
    DOWNLOAD "${url}" "${path}"
    EXPECTED_HASH SHA256=${hash}
    STATUS status
  )
  list(GET status 0 code)
  if (NOT code EQUAL 0)
    list(GET status 1 text)
    message(
      FATAL_ERROR
        "OPENBLACK_SENTRY: the configure step cannot get ${url}: ${text}"
    )
  endif ()
endfunction ()

openblack_sentry_browser_download(
  "https://browser.sentry-cdn.com/${OPENBLACK_SENTRY_BROWSER_VERSION}/bundle.min.js"
  "${OPENBLACK_SENTRY_BROWSER_SCRIPT}"
  "${OPENBLACK_SENTRY_BROWSER_SHA256}"
)
openblack_sentry_browser_download(
  "https://raw.githubusercontent.com/getsentry/sentry-javascript/${OPENBLACK_SENTRY_BROWSER_VERSION}/LICENSE"
  "${OPENBLACK_SENTRY_BROWSER_LICENSE}"
  "${OPENBLACK_SENTRY_BROWSER_LICENSE_SHA256}"
)

# The origin that gets the reports of a DSN, for the connect-src of the content security policy of the page: the scheme,
# the host and the port of the DSN. Empty when the DSN is empty or has an unknown form. The origin holds no key.
function (openblack_sentry_report_origin dsn out)
  set(origin "")
  if (dsn MATCHES "^(https?)://[^@/]+@([^/]+)/")
    set(origin "${CMAKE_MATCH_1}://${CMAKE_MATCH_2}")
  endif ()
  set(${out}
      "${origin}"
      PARENT_SCOPE
  )
endfunction ()
