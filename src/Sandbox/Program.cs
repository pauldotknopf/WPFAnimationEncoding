using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WPFAnimationEncoding;

namespace Sandbox
{
    class Program
    {
        static void Main(string[] args)
        {
            using (var file = new VideoReEncoder())
            {
                var outputFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264out.mov");
                if(File.Exists(outputFile))
                    File.Delete(outputFile);

                file.StartReEncoding(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264.mov"), 
                    outputFile,
                    (Bitmap image, ref bool cancel) =>
                    {
                        return null;
                    });
            }
        }
    }
}
