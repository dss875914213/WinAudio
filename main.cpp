// ����������WAS�ɼ���Ƶ��demo
/*
References:
https://msdn.microsoft.com/en-us/library/dd370800(v=vs.85).aspx
http://blog.csdn.net/leave_rainbow/article/details/50917043
http://blog.csdn.net/lwsas1/article/details/46862195?locationNum=1
WindowsSDK7-Samples-master\multimedia\audio\CaptureSharedEventDriven
*/
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <iostream>
using namespace std;

// ns(nanosecond) : ���룬ʱ�䵥λ��һ���ʮ�ڷ�֮һ
// 1��=1000����; 1����=1000΢��; 1΢��=1000����

// The REFERENCE_TIME data type defines the units for reference times in DirectShow. 
// Each unit of reference time is 100 nanoseconds.(100����Ϊһ��REFERENCE_TIMEʱ�䵥λ)

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC       (10000000)
#define REFTIMES_PER_MILLISEC  (10000)

#define EXIT_ON_ERROR(hres)  \
	if (FAILED(hres)) { goto Exit; }

#define SAFE_RELEASE(punk)  \
	if ((punk) != NULL)  \
				{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID   IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID   IID_IAudioClient = __uuidof(IAudioClient);
const IID   IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

#define MoveMemory RtlMoveMemory
#define CopyMemory RtlCopyMemory
#define FillMemory RtlFillMemory
#define ZeroMemory RtlZeroMemory

#define min(a,b)            (((a) < (b)) ? (a) : (b))

struct WAVEHEADER
{
	DWORD   dwRiff;                     // "RIFF"
	DWORD   dwSize;                     // Size
	DWORD   dwWave;                     // "WAVE"
	DWORD   dwFmt;                      // "fmt "
	DWORD   dwFmtSize;                  // Wave Format Size
};

//  Static RIFF header, we'll append the format to it.
const BYTE WaveHeader[] =
{
	'R',   'I',   'F',   'F',  0x00,  0x00,  0x00,  0x00, 'W',   'A',   'V',   'E',   'f',   'm',   't',   ' ', 0x00, 0x00, 0x00, 0x00
};

//  Static wave DATA tag.
const BYTE WaveData[] = { 'd', 'a', 't', 'a' };

//
//  Write the contents of a WAV file.  We take as input the data to write and the format of that data.
//
bool WriteWaveFile(HANDLE FileHandle, const BYTE* Buffer, const size_t BufferSize, const WAVEFORMATEX* WaveFormat)
{
	DWORD waveFileSize = sizeof(WAVEHEADER) + sizeof(WAVEFORMATEX) + WaveFormat->cbSize + sizeof(WaveData) + sizeof(DWORD) + static_cast<DWORD>(BufferSize);
	BYTE* waveFileData = new (std::nothrow) BYTE[waveFileSize];
	BYTE* waveFilePointer = waveFileData;
	WAVEHEADER* waveHeader = reinterpret_cast<WAVEHEADER*>(waveFileData);

	if (waveFileData == NULL)
	{
		printf("Unable to allocate %d bytes to hold output wave data\n", waveFileSize);
		return false;
	}

	//
	//  Copy in the wave header - we'll fix up the lengths later.
	//
	CopyMemory(waveFilePointer, WaveHeader, sizeof(WaveHeader));
	waveFilePointer += sizeof(WaveHeader);

	//
	//  Update the sizes in the header.
	//
	waveHeader->dwSize = waveFileSize - (2 * sizeof(DWORD));
	waveHeader->dwFmtSize = sizeof(WAVEFORMATEX) + WaveFormat->cbSize;

	//
	//  Next copy in the WaveFormatex structure.
	//
	CopyMemory(waveFilePointer, WaveFormat, sizeof(WAVEFORMATEX) + WaveFormat->cbSize);
	waveFilePointer += sizeof(WAVEFORMATEX) + WaveFormat->cbSize;


	//
	//  Then the data header.
	//
	CopyMemory(waveFilePointer, WaveData, sizeof(WaveData));
	waveFilePointer += sizeof(WaveData);
	*(reinterpret_cast<DWORD*>(waveFilePointer)) = static_cast<DWORD>(BufferSize);
	waveFilePointer += sizeof(DWORD);

	//
	//  And finally copy in the audio data.
	//
	CopyMemory(waveFilePointer, Buffer, BufferSize);

	//
	//  Last but not least, write the data to the file.
	//
	DWORD bytesWritten;
	if (!WriteFile(FileHandle, waveFileData, waveFileSize, &bytesWritten, NULL))
	{
		printf("Unable to write wave file: %d\n", GetLastError());
		delete[]waveFileData;
		return false;
	}

	if (bytesWritten != waveFileSize)
	{
		printf("Failed to write entire wave file\n");
		delete[]waveFileData;
		return false;
	}
	delete[]waveFileData;
	return true;
}

//
//  Write the captured wave data to an output file so that it can be examined later.
//
void SaveWaveData(BYTE* CaptureBuffer, size_t BufferSize, const WAVEFORMATEX* WaveFormat)
{
	HRESULT hr = NOERROR;

	SYSTEMTIME st;
	GetLocalTime(&st);
	char waveFileName[_MAX_PATH] = { 0 };
	sprintf_s(waveFileName, ".\\WAS_%04d-%02d-%02d_%02d_%02d_%02d_%02d.wav",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	HANDLE waveHandle = CreateFile(waveFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);
	if (waveHandle != INVALID_HANDLE_VALUE)
	{
		if (WriteWaveFile(waveHandle, CaptureBuffer, BufferSize, WaveFormat))
		{
			printf("Successfully wrote WAVE data to %s\n", waveFileName);
		}
		else
		{
			printf("Unable to write wave file\n");
		}
		CloseHandle(waveHandle);
	}
	else
	{
		printf("Unable to open output WAV file %s: %d\n", waveFileName, GetLastError());
	}

}

#define DEF_CAPTURE_MIC
/*
ע1: ����ʱ ���0
ע2: ����ʱ Ӧ�ý�¼���豸�е���˷���ΪĬ���豸
ע3: ����DEF_CAPTURE_MICʱ�����Բɼ���˷� ������Բɼ�������
ע4:
���Բɼ�����:
Initializeʱ��Ҫ����AUDCLNT_STREAMFLAGS_LOOPBACK
����ģʽ�£���Ƶengine�Ὣrending�豸���ڲ��ŵ���Ƶ���� ����һ�ݵ���Ƶ��endpoint buffer
�����Ļ���WASAPI client���Բɼ���the stream.
��ʱ���ɼ���Speaker������
*/

int main(int argc, char* argv[])
{
	HRESULT hr;
	int  nCnt = 0;

	size_t nCaptureBufferSize = 8 * 1024 * 1024;
	size_t nCurrentCaptureIndex = 0;

	BYTE* pbyCaptureBuffer = new (std::nothrow) BYTE[nCaptureBufferSize];
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDevice* pDevice = NULL;
	IAudioClient* pAudioClient = NULL;
	IAudioCaptureClient* pCaptureClient = NULL;
	WAVEFORMATEX* pwfx = NULL;

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	UINT32         bufferFrameCount;
	UINT32         numFramesAvailable;
	int nFrameSize;
	BYTE* pData;
	UINT32         packetLength = 0;
	DWORD          flags;
	HANDLE hAudioSamplesReadyEvent;
	bool stillPlaying = true;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		printf("Unable to initialize COM in thread: %x\n", hr);
		return hr;
	}

	// ����ö�������Ƶ�豸
	// ����������ʱ���ȡ������������п��õ��豸����ָ������Ҫ�õ����Ǹ�����
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator,
		NULL,
		CLSCTX_ALL,
		IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

		hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice); // �ɼ���˷�
	EXIT_ON_ERROR(hr)

		// ����һ���������ͨ�������Ի�ȡ������Ҫ��һ������
		hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->GetMixFormat(&pwfx);
	EXIT_ON_ERROR(hr)
		printf("\nGetMixFormat...\n");
	cout << "wFormatTag      : " << pwfx->wFormatTag << endl
		<< "nChannels       : " << pwfx->nChannels << endl
		<< "nSamplesPerSec  : " << pwfx->nSamplesPerSec << endl
		<< "nAvgBytesPerSec : " << pwfx->nAvgBytesPerSec << endl
		<< "nBlockAlign     : " << pwfx->nBlockAlign << endl
		<< "wBitsPerSample  : " << pwfx->wBitsPerSample << endl
		<< "cbSize          : " << pwfx->cbSize << endl << endl;

	nFrameSize = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;

	cout << "nFrameSize           : " << nFrameSize << " Bytes" << endl
		<< "hnsRequestedDuration : " << hnsRequestedDuration
		<< " REFERENCE_TIME time units. ��(" << hnsRequestedDuration / 10000 << "ms)" << endl;

	// ��ʼ�������������������ָ��������󻺳������ȣ��������Ҫ��
	// Ӧ�ó���������ݿ�Ĵ�С�Լ���ʱ���̶�������ĳ�ʼ�������������ҿ����ĵ�����
	// https://msdn.microsoft.com/en-us/library/dd370875(v=vs.85).aspx
	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);
	EXIT_ON_ERROR(hr)
		REFERENCE_TIME hnsStreamLatency;
	hr = pAudioClient->GetStreamLatency(&hnsStreamLatency);
	EXIT_ON_ERROR(hr)

		cout << "GetStreamLatency     : " << hnsStreamLatency
		<< " REFERENCE_TIME time units. ��(" << hnsStreamLatency / 10000 << "ms)" << endl;
	REFERENCE_TIME hnsDefaultDevicePeriod;
	REFERENCE_TIME hnsMinimumDevicePeriod;
	hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsMinimumDevicePeriod);
	EXIT_ON_ERROR(hr)

		cout << "GetDevicePeriod  ...\n"
		<< "hnsDefaultDevicePeriod : " << hnsDefaultDevicePeriod
		<< " REFERENCE_TIME time units. ��(" << hnsDefaultDevicePeriod / 10000 << "ms)" << endl
		<< "hnsMinimumDevicePeriod : " << hnsMinimumDevicePeriod
		<< " REFERENCE_TIME time units. ��(" << hnsMinimumDevicePeriod / 10000 << "ms)" << endl;

	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)
		cout << endl << "GetBufferSize        : " << bufferFrameCount << endl;

	// SetEventHandle
	//////////////////////////////////////////////////////////////////////////
	hAudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (hAudioSamplesReadyEvent == NULL)
	{
		printf("Unable to create samples ready event: %d.\n", GetLastError());
		goto Exit;
	}
	hr = pAudioClient->SetEventHandle(hAudioSamplesReadyEvent);
	if (FAILED(hr))
	{
		printf("Unable to set ready event: %x.\n", hr);
		return false;
	}
	//////////////////////////////////////////////////////////////////////////

	// �����ɼ�����ӿ�
	hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->Start();  // Start recording.
	EXIT_ON_ERROR(hr)

		printf("\nAudio Capture begin...\n\n");

	HANDLE waitArray[3];
	waitArray[0] = hAudioSamplesReadyEvent;

	// Each loop fills about half of the shared buffer.
	while (stillPlaying)
	{
		DWORD waitResult = WaitForMultipleObjects(1, waitArray, FALSE, INFINITE);
		switch (waitResult)
		{
		case WAIT_OBJECT_0 + 0:     // _AudioSamplesReadyEvent
			hr = pCaptureClient->GetNextPacketSize(&packetLength);
			EXIT_ON_ERROR(hr)

				printf("%06d # _AudioSamplesReadyEvent packetLength:%06u \n", nCnt, packetLength);

			while (packetLength != 0)
			{
				// Get the available data in the shared buffer.
				// ��������������ȡ����
				//  ��Ƶ֡�Ĵ�С�����ֽ�Ϊ��λ���������е�ͨ��������ÿ��ͨ���Ĳ�����С�� ���磬���ڰ��� 16 λ������������ (2 ����) ����֡��СΪ 4 ���ֽڡ�
				hr = pCaptureClient->GetBuffer(&pData,
					&numFramesAvailable,
					&flags, NULL, NULL);
				EXIT_ON_ERROR(hr)


					nCnt++;

				// test flags
				//////////////////////////////////////////////////////////////////////////
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					printf("AUDCLNT_BUFFERFLAGS_SILENT \n");
				}

				if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
				{
					printf("%06d # AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY \n", nCnt);
				}
				//////////////////////////////////////////////////////////////////////////

				UINT32 framesToCopy = min(numFramesAvailable, static_cast<UINT32>((nCaptureBufferSize - nCurrentCaptureIndex) / nFrameSize));
				if (framesToCopy != 0)
				{
					//
					//  The flags on capture tell us information about the data.
					//
					//  We only really care about the silent flag since we want to put frames of silence into the buffer
					//  when we receive silence.  We rely on the fact that a logical bit 0 is silence for both float and int formats.
					//
					if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
					{
						//
						//  Fill 0s from the capture buffer to the output buffer.
						//
						ZeroMemory(&pbyCaptureBuffer[nCurrentCaptureIndex], framesToCopy * nFrameSize);
					}
					else
					{
						//
						//  Copy data from the audio engine buffer to the output buffer.
						//
						CopyMemory(&pbyCaptureBuffer[nCurrentCaptureIndex], pData, framesToCopy * nFrameSize);
					}
					//
					//  Bump the capture buffer pointer.
					//
					nCurrentCaptureIndex += framesToCopy * nFrameSize;
				}

				hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
				EXIT_ON_ERROR(hr)

					hr = pCaptureClient->GetNextPacketSize(&packetLength);
				EXIT_ON_ERROR(hr)
					UINT32 ui32NumPaddingFrames;
				hr = pAudioClient->GetCurrentPadding(&ui32NumPaddingFrames);
				EXIT_ON_ERROR(hr)
					if (0 != ui32NumPaddingFrames)
					{
						printf("GetCurrentPadding : %6u\n", ui32NumPaddingFrames);
					}
				//////////////////////////////////////////////////////////////////////////

				// �ɼ�һ����Ŀ��buffer���˳�
				if (nCnt == 1000)
				{
					stillPlaying = false;
					break;
				}

			} // end of 'while (packetLength != 0)'

			break;
		} // end of 'switch (waitResult)'

	} // end of 'while (stillPlaying)'
	SaveWaveData(pbyCaptureBuffer, nCurrentCaptureIndex, pwfx);

	printf("\nAudio Capture Done.\n");

	hr = pAudioClient->Stop();  // Stop recording.
	EXIT_ON_ERROR(hr)

		Exit:
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
		SAFE_RELEASE(pDevice)
		SAFE_RELEASE(pAudioClient)
		SAFE_RELEASE(pCaptureClient)

		CoUninitialize();
	getchar();

	return 0;
}