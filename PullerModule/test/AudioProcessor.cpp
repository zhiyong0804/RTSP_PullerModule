/**
 * @file AudioProcessor.cpp
 * @brief 音频处理模块实现
 * @author kofera.deng <dengyi@comtom.cn>
 * @version 1.0.0
 * @date 2014-01-08
 */
#include "AudioProcessor.h"
#include "MutexLock.h"
#include <signal.h>
#include <string>

//DEFINE_STATIC_LOG(CAudioProcessor);
#define LogInfo printf
#define LogError printf
#define LogNotice printf

CAudioProcessor::CAudioProcessor ()
	: m_pcmHandle(NULL), m_pcmParams(NULL), m_pcmParamsSw(NULL), m_mixerHandle(NULL),
	m_curFmt(0), m_curSampleRate(0), m_curChannels(0), m_curVolume(80), 
	m_running(false), m_tid(0)
{
	pthread_mutex_init(&m_mutex, NULL);
	pthread_mutex_init(&m_waitMutex, NULL);
	pthread_cond_init(&m_cond, NULL);
	pthread_cond_init(&m_canPush, NULL);
	m_dataCache.clear();
	signal(SIGALRM, CAudioProcessor::alarmHandle);
}

CAudioProcessor::~CAudioProcessor ()
{
	pthread_mutex_destroy(&m_mutex);
	pthread_mutex_destroy(&m_waitMutex);
	pthread_cond_destroy(&m_cond);
	pthread_cond_destroy(&m_canPush);
}

CAudioProcessor* CAudioProcessor::instance()
{
	static CAudioProcessor* ap = NULL;
	if (ap == NULL)
		ap = new CAudioProcessor();

	return ap;
}

void CAudioProcessor::destroyProcessor()
{
	do
	{
		CMutexLock lock(&m_mutex);
		data_list_t::iterator it = m_dataCache.begin();
		
		if (it == m_dataCache.end())
		{
				break;
		}

		data_t pkt = *it;
		delete pkt.data;
		m_dataCache.erase(it);
	} while (true);

	delete this;
}

bool CAudioProcessor::start()
{
	return pthread_create(&m_tid, NULL, decodeThread, this) == 0;
}

void CAudioProcessor::stop()
{
	if (!m_running) return;
	
	LogInfo("Audio Processor stopping ...");
	m_running = false;
	
	CMutexLock lock(&m_mutex);
	pthread_cond_signal(&m_cond);
	lock.leave();

	pthread_join(m_tid, NULL);
}

int CAudioProcessor::initProcessor()
{
	int ret = 0, openCnt = 0;

	while((ret = snd_pcm_open(&m_pcmHandle, /*"hw:0,0"*/ "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) && m_running)
	{
		LogError ("open PCM device failed  %s !", snd_strerror(ret));
		m_pcmHandle = NULL;

		if (++openCnt > 3)
			return ret;
		
		sleep(1);
	}
	
	ret = snd_mixer_open(&m_mixerHandle, 0);
	if (ret < 0)
	{
		LogError("open Mixer device failed %s !", snd_strerror(ret));
		m_mixerHandle = NULL;
		return ret;
	}

	// Attach an HCTL to an opened mixer
	if ((ret = snd_mixer_attach(m_mixerHandle, "default" )) < 0)
	{
		LogError("snd_mixer_attach error %d !", ret);
	    snd_mixer_close(m_mixerHandle);
	    m_mixerHandle = NULL;
		return ret;
	}
	
	// 注册混音器
	if ((ret = snd_mixer_selem_register(m_mixerHandle, NULL, NULL )) < 0)
	{
	    LogError("snd_mixer_selem_register error %d !", ret);
	    snd_mixer_close(m_mixerHandle);
	    m_mixerHandle = NULL;
		return ret;
	}
	
	// 加载混音器
	if ((ret = snd_mixer_load(m_mixerHandle)) < 0)
	{
		LogError("snd_mixer_load error %d !", ret);
	    snd_mixer_close(m_mixerHandle);
	    m_mixerHandle = NULL;
		return ret;
	}

	setVolume(m_curVolume);
	
	return ret;
}

int CAudioProcessor::releaseProcessor()
{
	int ret = 0;

	if (m_pcmParamsSw != NULL)
	{
		snd_pcm_sw_params_free (m_pcmParamsSw); 
		m_pcmParamsSw = NULL;
	}

	if (m_pcmParams != NULL)
	{
		snd_pcm_hw_params_free (m_pcmParams);
		m_pcmParams = NULL;
	}

	if (m_pcmHandle != NULL)
	{
		snd_pcm_drop(m_pcmHandle);
		snd_pcm_close(m_pcmHandle);
		m_curSampleRate = 0;
		m_curFmt = 0;
		m_curChannels = 0;
		m_pcmHandle = NULL;
	}

	if (m_mixerHandle != NULL)
	{
		snd_mixer_close(m_mixerHandle);
		m_mixerHandle = NULL;
	}

	return ret;
}

bool CAudioProcessor::isWorking() const
{
	return m_dataCache.size() > 0;
}

void CAudioProcessor::deliverAudioData(uint8_t* data, int dataSize)
{
	if (data == NULL || !m_running) return;

	while (m_dataCache.size() > 20)
	{
		CMutexLock lock(&m_mutex);
		pthread_cond_wait(&m_canPush, &m_mutex);
	}

	data_t dataPkt;
	dataPkt.data = new uint8_t[dataSize];
	dataPkt.dataSize = dataSize;

	memcpy(dataPkt.data, data, dataSize);

	CMutexLock lock(&m_mutex);
	m_dataCache.push_back(dataPkt);
	lock.leave();

//	if (m_dataCache.size() == 1)
//	{
		CMutexLock waitLock(&m_waitMutex);
		pthread_cond_signal(&m_cond);
		waitLock.leave();
//	}
}

int CAudioProcessor::readAudioData(uint8_t* buf, int readSize)
{
	int retSize = 0;

	if (m_running && m_dataCache.size() == 0)
	{
		// 此处加上信号触发用于避免由于解码模块重启导致缓冲队列为空出现的如下情形：
		// deliverAudioData处于信号等待状态而readAudioData因为队列为空会进行等待, 此时程序会出现死锁的情况
		CMutexLock lock(&m_mutex);
		pthread_cond_signal(&m_canPush);
		lock.leave();

		while (m_running && m_dataCache.size() < 1/* modified by dengyi at 2014-03-23 */)
		{
			
			CMutexLock waitLock(&m_waitMutex);
			pthread_cond_wait(&m_cond, &m_waitMutex);
		}
	}
	
	do
	{
		CMutexLock lock(&m_mutex);
		data_list_t::iterator it = m_dataCache.begin();
		
		if (it == m_dataCache.end())
		{
				break;
		}

		data_t pkt = *it;

		if ((retSize + pkt.dataSize) > readSize)
		{
			break;
		}
		else
		{
			memcpy(buf+retSize, pkt.data, pkt.dataSize);
			retSize += pkt.dataSize;
			delete pkt.data;
			m_dataCache.erase(it);
			break;
		}
	} while (0);

	CMutexLock lock(&m_waitMutex);
	pthread_cond_signal(&m_canPush);
	
	return retSize;
}

int CAudioProcessor::setVolume(int vol)
{
	if (m_mixerHandle == NULL || !m_running)
		return 0;

	snd_mixer_elem_t *elem = NULL;
	for(elem=snd_mixer_first_elem(m_mixerHandle); elem; elem=snd_mixer_elem_next(elem))
	{
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE && snd_mixer_selem_is_active(elem))
		{
#if defined(_X86)
			if(std::string(snd_mixer_selem_get_name(elem)) == "Master") 
#else
			if(std::string(snd_mixer_selem_get_name(elem)) == "MV") 
#endif	
			{
				snd_mixer_selem_set_playback_volume_range(elem, 0, 100);
				snd_mixer_selem_set_playback_volume_all(elem, (long)vol);
				m_curVolume = vol;
			}					        
		}
	}

	return 0;
}


int CAudioProcessor::getVolume()
{
	if (m_mixerHandle == NULL || !m_running)
		return 0;

	long ll = 0, lr = 0;
	
	//处理事件
	snd_mixer_handle_events(m_mixerHandle);
	
	snd_mixer_elem_t *elem = NULL;
	for(elem=snd_mixer_first_elem(m_mixerHandle); elem; elem=snd_mixer_elem_next(elem))
	{
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE && snd_mixer_selem_is_active(elem))
		{
#if defined(_X86)
			if(std::string(snd_mixer_selem_get_name(elem)) == "Master") 
#else
			if(std::string(snd_mixer_selem_get_name(elem)) == "MV") 
#endif	
			{
				//左声道
				snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &ll);
				//右声道
				snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &lr);
				
				break;	
			}					        
		}
	}

	return (ll + lr) >> 1;
}


int CAudioProcessor::scale(mad_fixed_t sample)
{
	sample += (1L << (MAD_F_FRACBITS - 16));
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

#define OUTPUT_BUFFER_SIZE 8192
#define MAX_UNDERRUN 10 

int CAudioProcessor::playAudioFrame(struct mad_pcm *pcm)
{
	unsigned int nchannels, nsamples, n;
	mad_fixed_t const *left_ch, *right_ch;
	unsigned char outputBuf[OUTPUT_BUFFER_SIZE] = {0} , *outputPtr;
	int fmt, wrote, speed, err;

	nchannels = pcm->channels;
	n = nsamples = pcm->length;
	left_ch = pcm->samples[0];
	right_ch = pcm->samples[1];

	fmt = SND_PCM_FORMAT_S16_LE;
	AudioProcessor->setAudioParams(fmt, pcm->samplerate, pcm->channels);

	outputPtr = outputBuf;
	while (nsamples--)
	{
		signed int sample;
		sample = scale(*left_ch++);
		*(outputPtr++) = sample >> 0;
		*(outputPtr++) = sample >> 8;

		if (nchannels == 2)
		{
			sample = scale(*right_ch++);
			*(outputPtr++) = sample >> 0;
			*(outputPtr++) = sample >> 8;
		}
		else if (nchannels == 1)
		{
			*(outputPtr++) = sample >> 0;
			*(outputPtr++) = sample >> 8;
		}
	}
	
	outputPtr = outputBuf;

 	if (m_pcmHandle != NULL &&  (err = snd_pcm_writei(m_pcmHandle, outputPtr, n)) < 0)
	{
		err = snd_pcm_recover(m_pcmHandle, err, 1);
		
		if (err < 0) {
			LogInfo("playAudioFrame err %s \n", snd_strerror(err));
			return err;
		}
	}
	outputPtr = outputBuf;
	return 0;
}

void CAudioProcessor::setAudioParams(int &fmt, unsigned int &samplerate, unsigned short &channels)
{
	int ret =0;

	if (m_pcmHandle == NULL) return;

	if (channels == 1) channels = 2; //将单声道转为双声道

	if (m_curFmt == fmt && m_curSampleRate == samplerate && m_curChannels == channels)
	{
		return;
	}

	LogInfo("sample rate: %d , channels: %d\n", samplerate, channels);

	if (m_pcmParams != NULL)
	{
		snd_pcm_hw_params_free (m_pcmParams);
		m_pcmParams = NULL;
	}
	
	snd_pcm_hw_params_malloc(&m_pcmParams);
	ret = snd_pcm_hw_params_any(m_pcmHandle, m_pcmParams);
	if (ret < 0)
	{
		LogError("snd_pcm_hw_params_any error: %s !\n", snd_strerror(ret));
		return;
	}
	
	ret = snd_pcm_hw_params_set_access(m_pcmHandle, m_pcmParams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0)
	{
		LogError("snd_pcm_params_set_access error: %s !\n", snd_strerror(ret));
		return;
	}

	ret = snd_pcm_hw_params_set_format(m_pcmHandle, m_pcmParams, (snd_pcm_format_t)fmt);
	if (ret < 0)
	{
		LogError("snd_pcm_params_set_format error: %s !\n", snd_strerror(ret));
		return;
	}
	m_curFmt = fmt;

	ret = snd_pcm_hw_params_set_channels(m_pcmHandle, m_pcmParams, channels);
	if (ret < 0)
	{
		LogError("snd_pcm_params_set_channels error: %s !\n", snd_strerror(ret));
		return;
	}
	m_curChannels = channels;

	int dir=0;

	ret = snd_pcm_hw_params_set_rate_near(m_pcmHandle, m_pcmParams, &samplerate, &dir);
	if (ret < 0)
	{
		LogError("snd_pcm_hw_params_set_rate_near error: %s !\n", snd_strerror(ret));
		return;
	}
	m_curSampleRate = samplerate;
	
	ret = snd_pcm_hw_params(m_pcmHandle, m_pcmParams);
	if (ret < 0)
	{
		LogError("snd_pcm_hw_params error: %s !\n", snd_strerror(ret));
		return;
	}
	
	if (m_pcmParamsSw != NULL)
	{
		snd_pcm_sw_params_free (m_pcmParamsSw);
		m_pcmParamsSw = NULL;
	}
	
	snd_pcm_sw_params_malloc(&m_pcmParamsSw);
	/* set software parameters */
	ret = snd_pcm_sw_params_current(m_pcmHandle, m_pcmParamsSw);
	if (ret < 0)
	{
		LogError("unable to determine current software params: %s !\n", snd_strerror(ret));
	}
	
	snd_pcm_uframes_t frames = 2048;
	ret = snd_pcm_hw_params_get_period_size(m_pcmParams, &frames, NULL);
	if (ret < 0)
	{
		LogError("unable get period size: %s !\n", snd_strerror(ret));
	}
	LogInfo("frames %d !\n", frames);

	ret = snd_pcm_sw_params_set_start_threshold(m_pcmHandle, m_pcmParamsSw, frames*2);
	if (ret < 0)
	{
		LogError("unable set start threshold: %s !\n", snd_strerror(ret));
	}

	if (ret = snd_pcm_sw_params(m_pcmHandle, m_pcmParamsSw) < 0)
	{
		LogError("unable set sw params: %s !\n", snd_strerror(ret));
	}
}

void* CAudioProcessor::decodeThread(void* param)
{
	CAudioProcessor* ap = (CAudioProcessor *)param;
	if (ap == NULL) return ap;

	ap->decode();
	return ap;
}

#define INPUT_BUFFER_SIZE (8192)

void CAudioProcessor::decode()
{
	do
	{
		struct mad_stream stream;
		struct mad_frame frame;
		struct mad_synth synth;

		uint8_t inputBuf[INPUT_BUFFER_SIZE] = {0};

		mad_stream_init(&stream);
		mad_frame_init(&frame);
		mad_synth_init(&synth);

		m_running = true;

		while (m_running)
		{
			// get decode data
			if (stream.buffer == NULL || stream.error == MAD_ERROR_BUFLEN)
			{
				int readSize = 0, remainSize = 0;
				uint8_t *readStart = NULL;

				if (stream.next_frame != NULL)
				{
					remainSize = stream.bufend - stream.next_frame;
					memmove(inputBuf, stream.next_frame, remainSize);
					readStart = inputBuf + remainSize;
					readSize = INPUT_BUFFER_SIZE - remainSize;
				}
				else
				{
					readSize = INPUT_BUFFER_SIZE;
					readStart = inputBuf;
					remainSize = 0;
				}
				
				readSize = readAudioData(readStart, readSize);

				if (readSize == 0)
				{
					stream.error = MAD_ERROR_BUFLEN;
					continue;
				}

				mad_stream_buffer(&stream, inputBuf, readSize+remainSize);
				stream.error = (mad_error)0;
			}
			
			// decode frame 
			if (mad_frame_decode(&frame, &stream))
			{
				if (MAD_RECOVERABLE(stream.error))
				{
					if (stream.error != MAD_ERROR_LOSTSYNC)
					{
						LogNotice("mad_frame_decode wran : code %d !\n", stream.error);
					}
					continue;
				}
				else if (stream.error == MAD_ERROR_BUFLEN)
				{
					continue;
				}
				else
				{
					LogError("mad_frame_decode error!\n");
					break;
				}
			}

			mad_synth_frame(&synth, &frame);

#if defined(HAVE_POWER_AMPLIFIER)	
			m_pa.turnOn();
#endif		
			if (m_pcmHandle == NULL && initProcessor() == 0)
				LogInfo("init the AudioProcessor done !\n");
			
			if (m_pcmHandle == NULL) continue;

			alarm(30);
			
			// play audio frame
			if(playAudioFrame(&synth.pcm) < 0)
			{
				LogError("rebooting the AudioProcessor !\n");
				releaseProcessor();
				LogInfo("release the AudioProcessor done !\n");
			}	
		}

		mad_synth_finish(&synth);
		mad_frame_finish(&frame);
		mad_stream_finish(&stream);

	} while (0);

	releaseProcessor();
	LogInfo("Audio Processor stopped !\n");
}

void CAudioProcessor::alarmHandle(int signo)
{
#if defined(HAVE_POWER_AMPLIFIER)
	AudioProcessor->m_pa.turnOff();
#endif
	AudioProcessor->releaseProcessor();
}
