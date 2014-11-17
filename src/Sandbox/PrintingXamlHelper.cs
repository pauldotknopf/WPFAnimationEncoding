using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Threading;

namespace Sandbox
{
    public static class PrintingXamlHelper
    {
        private static readonly object LockObject = new object();
        private static Dispatcher _dispatcher;

        static PrintingXamlHelper()
        {
        }

        private static void EnsureDispatcherCreated()
        {
            if (_dispatcher != null) return;

            lock (LockObject)
            {
                var thread = new Thread(() =>
                {
                    lock (LockObject)
                    {
                        _dispatcher = Dispatcher.CurrentDispatcher;
                        _dispatcher.Thread.IsBackground = true;
                        _dispatcher.UnhandledException += (sender, e) =>
                        {
                            e.Handled = true;
                        };
                        Monitor.Pulse(LockObject);
                    }
                    Dispatcher.Run();
                });
                thread.SetApartmentState(ApartmentState.STA);
                thread.Start();

                Monitor.Wait(LockObject);
            }
        }

        public static void Invoke(Action action)
        {
            EnsureDispatcherCreated();

            if (_dispatcher.Thread.ManagedThreadId == Thread.CurrentThread.ManagedThreadId)
            {
                action();
            }
            else
            {
                Exception exception = null;
                _dispatcher.Invoke(new Action(() =>
                {
                    try
                    {
                        action();
                    }
                    catch (Exception ex)
                    {
                        exception = ex;
                    }
                }));
                if (exception != null)
                    throw new Exception(exception.Message, exception);
            }
        }

        public static void BeginInvoke(Action action)
        {
            EnsureDispatcherCreated();

            if (_dispatcher.Thread.ManagedThreadId == Thread.CurrentThread.ManagedThreadId)
            {
                action();
            }
            else
            {
                Exception exception = null;
                _dispatcher.BeginInvoke(new Action(() =>
                {
                    try
                    {
                        action();
                    }
                    catch (Exception ex)
                    {
                        exception = ex;
                    }
                }));
                if (exception != null)
                    throw new Exception(exception.Message, exception);
            }
        }
    }
}
