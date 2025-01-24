# Computer Networks - Computer Networks (2nd Project)

> Curricular Unit: [Computer Networks - 2024/25 1S](https://sigarra.up.pt/feup/en/UCURR_GERAL.FICHA_UC_VIEW?pv_ocorrencia_id=541890)<br>
> Faculty: [FEUP](https://sigarra.up.pt/feup/en/web_page.Inicial)<br>
> Professor: [Helder Fontes](https://sigarra.up.pt/feup/en/func_geral.formview?p_codigo=682981)<br>
> Authors: [Bruno Oliveira](https://github.com/Process-ing), [Rodrigo Silva](https://github.com/racoelhosilva)<br>
> Final Grade: 20.0/20

## Description

This project focused on the configuration, usage and analysis of computer networks. It was divided into two parts.

The first part consisted in the development of a simple FTP application, able to download a select file from a FTP server. The application was developed in C and used the FTP protocol to communicate with the server.

The second part focused on the core of this project - to setup a computer network in multiple configuration and analyze their effects on the connectivity and performance of the network. This part took place on a computer lab specifically prepared for this project, and was divided into a total of 6 experiments, to be developed throughout the practical classes. In these experiments, we had to physically connect the computers (by configuring Ethernet cables, switches and routers), adjust some networking parameters on Linux machines and analyze the network traffic using multiple tools, such as `ping`, `traceroute` and Wireshark. The results of each experiment (logs) were then analyzed and discussed in the report.

## Run Instructions

To run the project, run the following commands in the Linux terminal:

```sh
make                  # Compile the program (saved in bin/)
cd bin                
./download <ftp-url>  # Download a file matching the FTP URL provided
```

## Tips and Tricks (for anyone doing a similar project)

- For the FTP application, some of the tips mentioned in the previous project still apply: avoid using the heap, avoid using exotic libraries, **try to have it ready at least 1 week before the deadline**, ...
- It is important that the FTP protocol is followed precisely. There is a slide nearly at the end of the specification showing a normal interaction with the server, and also explains well the messages that need to be exchanged and how to parse the responses, try to focus on that.
- *Use CRLF on the messages (use `\r\n` instead of `\n`)*. This is the standard and some servers do not work correctly without it.
- When doing the experiments, do not be afraid to look at the commands other people used in the project, like ours (seriously, some steps in the specification are very ambiguous and sometimes even incorrect), and try to confirm your results with your professor.
- Each time you go to a bench, the first thing you should do is reset the tuxes, the switch and the router (if needed), or at least restart the `networking` service. If you see that some part is not working correctly, like the router or something else, try to go to another bench. Some parts simply do not work some days and it is not worth losing time trying to fix them.
