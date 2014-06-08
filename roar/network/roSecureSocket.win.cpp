#include "pch.h"
#include "roSecureSocket.h"
#include "../base/roIOStream.h"
#include "../base/roLog.h"
#include "../base/roTypeCast.h"
#include "../base/roMutex.h"

#define SECURITY_WIN32
#include <schannel.h>
#include <sspi.h>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")

// Reference:
// http://www.coastrd.com/tls-with-schannel
// http://msdn.microsoft.com/en-us/library/aa379449
// http://www.drdobbs.com/implementing-an-ssltls-enabled-clientser/184401688
// http://www.codeproject.com/Articles/2642/SSL-TLS-client-server-for-NET-and-SSL-tunnelling
// Download Windows Server 2003 SP1 Platform SDK ISO from:
// http://www.microsoft.com/en-us/download/confirmation.aspx?id=15656

namespace ro {

static roSize _loadCount = 0;
static Mutex _loadMutex;
static roStatus _loadSecurityInterface()
{
	ScopeLock lock(_loadMutex);

	if(_loadCount > 0)
		return roStatus::ok;

	roStatus st;
	++_loadCount;
	st = roStatus::ok;
	return st;
}

static roStatus _unloadSecurityInterface()
{
	ScopeLock lock(_loadMutex);

	if(_loadCount > 0)
		--_loadCount;

	if(_loadCount > 0)
		return roStatus::ok;

	roStatus st;
	st = roStatus::ok;
	return st;
}

SecureSocket::SecureSocket()
{

}

roStatus SecureSocket::create()
{
	return _socket.create(BsdSocket::TCP);
}

roStatus SecureSocket::connect(const SockAddr& endPoint)
{
	roStatus st;

	roVerify(_loadSecurityInterface());

	HCERTSTORE hMyCertStore = NULL;
	hMyCertStore = CertOpenSystemStoreA(0, "MY");
	// TODO: Log

	PCCERT_CONTEXT pCertContext = NULL;
	const char* username = NULL;
	if(username) {
		pCertContext = CertFindCertificateInStore(
			hMyCertStore, 
			X509_ASN_ENCODING,
			0,
			CERT_FIND_SUBJECT_STR_A,
			username,
			NULL);
		// TODO: Log
	}

	SCHANNEL_CRED SchannelCred;
	ZeroMemory(&SchannelCred, sizeof(SchannelCred));

	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	if(pCertContext) {
		SchannelCred.cCreds = 1;
		SchannelCred.paCred = &pCertContext;
	}

	DWORD dwProtocol = 0;
//	dwProtocol = SP_PROT_PCT1;
//	dwProtocol = SP_PROT_SSL2;
//	dwProtocol = SP_PROT_SSL3;
//	dwProtocol = SP_PROT_TLS1;
	SchannelCred.grbitEnabledProtocols = dwProtocol;

	ALG_ID aiKeyExch = 0;
//	aiKeyExch = CALG_RSA_KEYX;
//	aiKeyExch = CALG_DH_EPHEM;

	DWORD cSupportedAlgs = 0;
	ALG_ID rgbSupportedAlgs[16];

	if(aiKeyExch)
		rgbSupportedAlgs[cSupportedAlgs++] = aiKeyExch;

	if(cSupportedAlgs) {
		SchannelCred.cSupportedAlgs = cSupportedAlgs;
		SchannelCred.palgSupportedAlgs = rgbSupportedAlgs;
	}

	SchannelCred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS;

	CtxtHandle hContext;
	CredHandle hClientCreds;
	TimeStamp tsExpiry;
	PSecurityFunctionTableA g_pSSPI = InitSecurityInterfaceA();
	SECURITY_STATUS secStatus = g_pSSPI->AcquireCredentialsHandleA(
		NULL,					// Name of principal    
		UNISP_NAME_A,			// Name of package
		SECPKG_CRED_OUTBOUND,	// Flags indicating use
		NULL,					// Pointer to logon ID
		&SchannelCred,			// Package specific data
		NULL,					// Pointer to GetKey() func
		NULL,					// Value to pass to GetKey()
		&hClientCreds,			// (out) Cred Handle
		&tsExpiry				// (out) Lifetime (optional)
	);
	if(secStatus != SEC_E_OK) {
		return st;
	}

	// Perform hand shake
	const DWORD dwSSPIFlags =
		ISC_REQ_SEQUENCE_DETECT   |
		ISC_REQ_REPLAY_DETECT     |
		ISC_REQ_CONFIDENTIALITY   |
		ISC_RET_EXTENDED_ERROR    |
		ISC_REQ_ALLOCATE_MEMORY   |
		ISC_REQ_STREAM;

	{
		DWORD dwSSPIOutFlags;

		SecBufferDesc outBuffer;
		SecBuffer outBuffers[1];
		outBuffers[0].pvBuffer   = NULL;
		outBuffers[0].BufferType = SECBUFFER_TOKEN;
		outBuffers[0].cbBuffer   = 0;
		outBuffer.cBuffers = 1;
		outBuffer.pBuffers = outBuffers;
		outBuffer.ulVersion = SECBUFFER_VERSION;

		secStatus = g_pSSPI->InitializeSecurityContextA(
			&hClientCreds,
			NULL,
			"google.com",
			dwSSPIFlags,
			0,
			SECURITY_NATIVE_DREP,
			NULL,
			0,
			&hContext,
			&outBuffer,
			&dwSSPIOutFlags,
			&tsExpiry
		);
		if(secStatus != SEC_I_CONTINUE_NEEDED) {
			return st;
		}

		// Connect to the server
		st = _socket.connect(endPoint);
		if(!st) return st;

		// Send response to server if there is one.
		if(outBuffers[0].cbBuffer && outBuffers[0].pvBuffer) {
			roSize len = outBuffers[0].cbBuffer;
			st = _socket.send(outBuffers[0].pvBuffer, len);
			if(!st) return st;
		}
	}

	// Read response from server
	{
		roSize IO_BUFFER_SIZE = 0x10000;

		DWORD dwSSPIOutFlags;
		SecBufferDesc inBuffer;
		SecBuffer inBuffers[2];
		SecBufferDesc outBuffer;
		SecBuffer outBuffers[1];

		ByteArray buf;
		st = buf.resizeNoInit(IO_BUFFER_SIZE);
		if(!st) return st;

		SECURITY_STATUS scRet = SEC_I_CONTINUE_NEEDED;
		while(scRet == SEC_I_CONTINUE_NEEDED ||
			  scRet == SEC_E_INCOMPLETE_MESSAGE ||
			  scRet == SEC_I_INCOMPLETE_CREDENTIALS) 
		{
			roSize len = buf.sizeInByte();
			st = _socket.receive(buf.bytePtr(), len);
			if(!st) return st;
			roVerify(buf.resize(len));

			// Set up the input buffers. Buffer 0 is used to pass in data
			// received from the server. Schannel will consume some or all
			// of this. Leftover data (if any) will be placed in buffer 1 and
			// given a buffer type of SECBUFFER_EXTRA.
			inBuffers[0].pvBuffer = buf.bytePtr();
			inBuffers[0].cbBuffer = buf.sizeInByte();
			inBuffers[0].BufferType = SECBUFFER_TOKEN;

			inBuffers[1].pvBuffer = NULL;
			inBuffers[1].cbBuffer = 0;
			inBuffers[1].BufferType = SECBUFFER_EMPTY;

			inBuffer.cBuffers = 2;
			inBuffer.pBuffers = inBuffers;
			inBuffer.ulVersion = SECBUFFER_VERSION;

			outBuffers[0].pvBuffer = NULL;
			outBuffers[0].BufferType = SECBUFFER_TOKEN;
			outBuffers[0].cbBuffer = 0;

			outBuffer.cBuffers = 1;
			outBuffer.pBuffers = outBuffers;
			outBuffer.ulVersion = SECBUFFER_VERSION;

			secStatus = g_pSSPI->InitializeSecurityContextA(
				&hClientCreds,
				&hContext,
				NULL,
				dwSSPIFlags,
				0,
				SECURITY_NATIVE_DREP,
				&inBuffer,
				0,
				NULL,
				&outBuffer,
				&dwSSPIOutFlags,
				&tsExpiry
			);

			if (secStatus == SEC_E_OK ||
				secStatus == SEC_I_CONTINUE_NEEDED ||
				FAILED(secStatus) && (dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR))
			{
				secStatus = secStatus;
			}

			if(secStatus != SEC_I_CONTINUE_NEEDED) {
				return st;
			}
		}
	}

	return roStatus::ok;
}

SecureSocket::~SecureSocket()
{
	roVerify(_unloadSecurityInterface());
}

}	// namespace ro
