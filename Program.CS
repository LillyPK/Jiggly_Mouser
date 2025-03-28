using System;
using System.Threading;
using System.Runtime.InteropServices;

class MouseMover
{
    // P/Invoke imports for Windows API
    [DllImport("user32.dll")]
    static extern bool SetCursorPos(int X, int Y);

    [DllImport("user32.dll")]
    static extern int GetSystemMetrics(int nIndex);

    // System metrics constants
    private const int SM_CXSCREEN = 0;
    private const int SM_CYSCREEN = 1;

    // Program variables
    private static int screenWidth;
    private static int screenHeight;
    private static int centerX;
    private static int centerY;
    private static int radius = 200;
    private static int intervalSeconds;
    private static int startDelay;
    private static volatile bool running = true;
    private static Random random = new Random();

    static void Main(string[] args)
    {
        // Get screen dimensions
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        centerX = screenWidth / 2;
        centerY = screenHeight / 2;

        Console.WriteLine("Enter the seconds between mouse movements: ");
        string? intervalInput = Console.ReadLine();
        intervalSeconds = !string.IsNullOrEmpty(intervalInput) ? int.Parse(intervalInput) : 5;

        Console.WriteLine("Enter the delay /s before starting: ");
        string? delayInput = Console.ReadLine();
        startDelay = !string.IsNullOrEmpty(delayInput) ? int.Parse(delayInput) : 3;

        // Start delay
        Thread.Sleep(startDelay * 1000);

        // Create and start the mouse mover thread
        Thread mouseMoverThread = new Thread(() => MoveMousePeriodically());
        mouseMoverThread.IsBackground = true;
        mouseMoverThread.Start();

        Console.WriteLine("Press Enter to stop the program...");
        Console.ReadLine();

        // Signal thread to stop and wait for it
        running = false;
        mouseMoverThread.Join();
    }

    static void MoveMousePeriodically()
    {
        while (running)
        {
            // Generate random angle and distance within circle
            double angle = random.NextDouble() * 2 * Math.PI;
            double distance = random.NextDouble() * radius;

            // Calculate new coordinates
            int newX = (int)(centerX + distance * Math.Cos(angle));
            int newY = (int)(centerY + distance * Math.Sin(angle));

            // Move the cursor
            SetCursorPos(newX, newY);

            // Wait for the specified interval
            Thread.Sleep(intervalSeconds * 1000);
        }
    }
}
