/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OwnCertificate.h"

#include <array>
#include <memory>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

namespace openblack::server::own_certificate
{
namespace
{

struct KeyDeleter
{
	void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};
struct CertificateDeleter
{
	void operator()(X509* certificate) const { X509_free(certificate); }
};
struct BioDeleter
{
	void operator()(BIO* bio) const { BIO_free(bio); }
};
struct NumberDeleter
{
	void operator()(BIGNUM* number) const { BN_free(number); }
};

using Key = std::unique_ptr<EVP_PKEY, KeyDeleter>;
using Certificate = std::unique_ptr<X509, CertificateDeleter>;
using Bio = std::unique_ptr<BIO, BioDeleter>;

/// The count of random bytes in the serial number of a certificate
constexpr size_t k_SerialBytes = 16;

[[nodiscard]] std::optional<std::string> TextOf(BIO* bio)
{
	char* data = nullptr;
	const auto size = BIO_get_mem_data(bio, &data);
	if (size <= 0 || data == nullptr)
	{
		return std::nullopt;
	}
	return std::string(data, static_cast<size_t>(size));
}

[[nodiscard]] std::optional<online::server_key::Pin> PinOfKey(EVP_PKEY* key)
{
	unsigned char* der = nullptr;
	const auto length = i2d_PUBKEY(key, &der);
	if (length <= 0 || der == nullptr)
	{
		return std::nullopt;
	}
	online::server_key::Pin pin {};
	SHA256(der, static_cast<size_t>(length), pin.data());
	OPENSSL_free(der);
	return pin;
}

[[nodiscard]] bool SetSerial(X509* certificate)
{
	std::array<unsigned char, k_SerialBytes> bytes {};
	if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
	{
		return false;
	}
	// A serial number is a positive number
	bytes[0] &= 0x7F;
	std::unique_ptr<BIGNUM, NumberDeleter> number(BN_bin2bn(bytes.data(), static_cast<int>(bytes.size()), nullptr));
	return number != nullptr && BN_to_ASN1_INTEGER(number.get(), X509_get_serialNumber(certificate)) != nullptr;
}

} // namespace

std::optional<OwnCertificate> Make(std::string_view name)
{
	Key key(EVP_EC_gen("P-256"));
	Certificate certificate(X509_new());
	if (key == nullptr || certificate == nullptr)
	{
		return std::nullopt;
	}
	auto* cert = certificate.get();
	const std::string commonName(name);
	auto* subject = X509_get_subject_name(cert);
	if (X509_set_version(cert, 2) != 1 || !SetSerial(cert) ||
	    X509_gmtime_adj(X509_getm_notBefore(cert), -k_ValidBeforeSeconds) == nullptr ||
	    X509_gmtime_adj(X509_getm_notAfter(cert), k_ValidAfterSeconds) == nullptr || X509_set_pubkey(cert, key.get()) != 1 ||
	    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(commonName.c_str()), -1,
	                               -1, 0) != 1 ||
	    X509_set_issuer_name(cert, subject) != 1 || X509_sign(cert, key.get(), EVP_sha256()) <= 0)
	{
		return std::nullopt;
	}
	const auto pin = PinOfKey(key.get());
	Bio certificateText(BIO_new(BIO_s_mem()));
	Bio keyText(BIO_new(BIO_s_mem()));
	if (!pin.has_value() || certificateText == nullptr || keyText == nullptr ||
	    PEM_write_bio_X509(certificateText.get(), cert) != 1 ||
	    PEM_write_bio_PrivateKey(keyText.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1)
	{
		return std::nullopt;
	}
	auto certificatePem = TextOf(certificateText.get());
	auto keyPem = TextOf(keyText.get());
	if (!certificatePem.has_value() || !keyPem.has_value())
	{
		return std::nullopt;
	}
	return OwnCertificate {.certificatePem = std::move(*certificatePem), .keyPem = std::move(*keyPem), .pin = *pin};
}

std::optional<online::server_key::Pin> PinOfCertificate(std::string_view certificatePem)
{
	Bio text(BIO_new_mem_buf(certificatePem.data(), static_cast<int>(certificatePem.size())));
	if (text == nullptr)
	{
		return std::nullopt;
	}
	Certificate certificate(PEM_read_bio_X509(text.get(), nullptr, nullptr, nullptr));
	if (certificate == nullptr)
	{
		return std::nullopt;
	}
	// The public key of the certificate belongs to the certificate, so it is not freed here
	auto* key = X509_get0_pubkey(certificate.get());
	return key != nullptr ? PinOfKey(key) : std::nullopt;
}

} // namespace openblack::server::own_certificate
