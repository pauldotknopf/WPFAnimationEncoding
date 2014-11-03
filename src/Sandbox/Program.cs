using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Sandbox
{
    class Program
    {
        static void Main(string[] args)
        {
            using (var file = new WPFAnimationEncoding.VideoFileReader())
            {
                file.Open(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264.mov"));

                var frame = file.ReadVideoFrame();
                int frameNumber = 0;

                while (frame != null)
                {
                    frameNumber++;
                    frame.Save(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "test" + frameNumber + ".bmp"));
                    frame = file.ReadVideoFrame();
                }
                
            }
        }
    }
}
