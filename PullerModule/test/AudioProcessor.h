/**
 * @file AudioProcessor.h
 * @brief  音频处理模块头文件 
 * @author kofera.deng <dengyi@comtom.cn>
 * @version 1.0.0
 * @date 2014-01-03
 */

#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <mad.h>
#include <alsa/asoundlib.h>
#include <stdint.h>
//#include "LogUtils.h"
#include "pthread.h"
#include <vector>

#define AudioProcessor  CAudioProcessor::instance()

#if defined(HAVE_POWER_AMPLIFIER)
#include "PowerAmplifier.h"
#endif

class CAudioProcessor
{
	public:
		bool	start();
		void	stop();
		void	deliverAudioData(uint8_t* data, int dataSize);
		int		setVolume(int vol);
		int		getVolume();
		bool	isWorking() const;

		static CAudioProcessor* instance();
		void    destroyProcessor();

	protected:
		CAudioProcessor();
		virtual ~CAudioProcessor();
        
		/**
		 * @brief 
		 *
		 * @param fmt
		 * @param samplerate
		 * @param channels
		 */
		void	setAudioParams(int &fmt, unsigned int &samplerate, unsigned short &channels);
		static void* decodeThread(void*);
		void	decode();
		int		readAudioData(uint8_t* buf, int readSize);
		int		scale(mad_fixed_t sample);
		int		playAudioFrame(struct mad_pcm *pcm);

		static void alarmHandle(int signo);

		int  initProcessor();
		int  releaseProcessor();

	private:
		snd_pcm_t* m_pcmHandle;
		snd_pcm_hw_params_t* m_pcmParams;
		snd_pcm_sw_params_t* m_pcmParamsSw;
		snd_mixer_t* m_mixerHandle;

		int m_curFmt;
		int m_curSampleRate;
		int m_curChannels;
		int m_curVolume;

		int m_running;
		
		pthread_mutex_t m_mutex;
		pthread_mutex_t m_waitMutex;
		pthread_cond_t m_cond;
		pthread_cond_t m_canPush;
	
		pthread_t m_tid;

#if defined(HAVE_POWER_AMPLIFIER)
		CPowerAmplifier m_pa;
#endif
		struct data_t
		{
			uint8_t* data;
			int dataSize;
		};
		
		typedef std::vector<data_t> data_list_t;
		data_list_t m_dataCache;
		
//		DECLARE_STATIC_LOG();		
};


#endif /* AUDIO_PROCESSOR_H */
